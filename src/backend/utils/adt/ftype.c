/*
 * ftype.c -- code to manage large object files ("mao large objects")
 */

#include <sys/file.h>

#include "tmp/c.h"
#include "tmp/postgres.h"
#include "access/heapam.h"
#include "access/tupdesc.h"
#include "access/htup.h"
#include "access/itup.h"
#include "access/skey.h"
#include "access/sdir.h"
#include "access/genam.h"
#include "catalog/pg_proc.h"
#include "storage/fd.h"
#include "storage/buf.h"
#include "utils/palloc.h"
#include "utils/rel.h"
#include "utils/log.h"

RcsId("$Header$");

typedef struct _f262desc {
    ObjectId f_oid;
    uint32 f_seqno;
    uint32 f_offset;
    Relation f_heap;
    Relation f_index;
    OidSeq f_oslow;
    OidSeq f_oshigh;
    TupleDescriptor f_hdesc;
    ItemPointerData f_htid;
    IndexScanDesc f_iscan;
} f262desc;

/*
 *  FBLKSIZ is the amount of user block data we store in a single tuple.
 *  A file tuple contains a tuple header, an oid, a sequence number, and
 *  a varlena struct containing the file data.  8092 bytes is chosen to
 *  guarantee that sufficient space exists for the header, oid, and seqno.
 *  100 bytes is more than enough, but I don't feel like doing the math.
 */

#define FBLKSIZ	8092
#define PGFILES "mao_files"
#define INDNAME "mao_index"
#define OidSeqLTProcedure	922
#define OidSeqGEProcedure	925

File
f262file(name, flags, mode)
    struct varlena *name;
    int flags;
    int mode;
{
    char *pathname;
    int length;
    File vfd;

    /* open the data file */
    length = name->vl_len - sizeof(name->vl_len);
    pathname = (char *) palloc(length + 1);
    (void) bcopy(VARDATA(name), pathname, length);
    pathname[length] = '\0';
    if ((vfd = PathNameOpenFile(pathname, flags, mode)) < 0) {
	elog(NOTICE, "f262file() cannot open %s", pathname);
	pfree(pathname);
	return (-1);
    }
    pfree(pathname);

    return (vfd);
}

ObjectId
fimport(name)
    struct varlena *name;
{
    File vfd;
    ObjectId foid;
    int fseqno;
    int nbytes;
    Relation r;
    Relation indrel;
    TupleDescriptor tupdesc;
    TupleDescriptor itupdesc;
    HeapTuple htup;
    IndexTuple itup;
    GeneralInsertIndexResult res;
    struct varlena *fdata;
    OidSeq os;
    Datum values[3];
    Datum ivalues[1];
    char nulls[3];

    if ((vfd = f262file(name, O_RDONLY, 0666)) < 0)
	return ((ObjectId) NULL);

    /* start at beginning of file */
    FileSeek(vfd, 0L, L_SET);

    /*
     *  Next allocate a varlena big enough to hold one tuple's worth
     *  of block data.  Each tuple gets an (oid, seqno, block data)
     *  triple.
     */

    fdata = (struct varlena *) palloc(sizeof(struct varlena) + FBLKSIZ);
    if (fdata == (struct varlena *) NULL) {
	elog(NOTICE, "fimport() cannot allocate varlena for file data.");
	return ((ObjectId) NULL);
    }

    /* open pg_files and prepare to insert new tuples into it */
    if ((r = heap_openr(PGFILES)) == (Relation) NULL) {
	elog(NOTICE, "fimport() cannot open %s", PGFILES);
	pfree(fdata);
	return ((ObjectId) NULL);
    }
    tupdesc = RelationGetTupleDescriptor(r);

    if ((indrel = index_openr(INDNAME)) == (Relation) NULL) {
	elog(NOTICE, "fimport() cannot open %s", INDNAME);
	pfree(fdata);
	(void) heap_close(r);
	return ((ObjectId) NULL);
    }
    itupdesc = RelationGetTupleDescriptor(indrel);

    /* get a unique identifier for this file data */
    foid = newoid();

    /* copy file data into the pg_files table */
    nulls[0] = nulls[1] = nulls[2] = ' ';
    fseqno = 0;
    values[0] = foid;
    while ((nbytes = FileRead(vfd, VARDATA(fdata), FBLKSIZ)) > 0) {
	values[1] = fseqno;
	VARSIZE(fdata) = nbytes + sizeof(fdata->vl_len);
	values[2] = PointerGetDatum(fdata);
	htup = heap_formtuple(3, tupdesc, &values[0], &nulls[0]);
	(void) heap_insert(r, htup, (double *) NULL);

	/* index heap tuple by foid, fseqno */
	os = (OidSeq) mkoidseq(foid, fseqno);
	ivalues[0] = PointerGetDatum(os);
	itup = index_formtuple(1, itupdesc, &ivalues[0], &nulls[0]);
	bcopy(&(htup->t_ctid), &(itup->t_tid), sizeof(ItemPointerData));
	res = index_insert(indrel, itup, (double *) NULL);

	if (res)
	    pfree(res);

	pfree(itup);
	pfree(htup);

	/* bump sequence number */
	fseqno++;
    }

    (void) heap_close(r);
    (void) index_close(indrel);
    (void) FileClose(vfd);

    return (foid);
}

f262desc *
f262open(foid)
    ObjectId foid;
{
    f262desc *f;
    ScanKeyEntryData fkey[2];

    /* open a 262 big file descriptor */
    f = (f262desc *) palloc(sizeof(f262desc));
    f->f_oid = foid;
    f->f_seqno = 0;
    f->f_offset = 0;
    f->f_heap = heap_openr(PGFILES);
    f->f_hdesc = RelationGetTupleDescriptor(f->f_heap);
    f->f_index = index_openr(INDNAME);
    (void) bzero((char *) &(f->f_htid), sizeof(f->f_htid));

    /* initialize the scan key */
    f->f_oslow = (OidSeq) mkoidseq(foid, 0);
    ScanKeyEntryInitialize(&fkey[0], 0x0, 1, OidSeqGEProcedure,
			   PointerGetDatum(f->f_oslow));
    f->f_oshigh = (OidSeq) mkoidseq(foid + 1, 0);
    ScanKeyEntryInitialize(&fkey[1], 0x0, 1, OidSeqLTProcedure,
			   PointerGetDatum(f->f_oshigh));

    f->f_iscan = index_beginscan(f->f_index, 0x0, 2, &fkey[0]);

    return (f);
}

int
f262seek(f, loc)
    f262desc *f;
    int loc;
{
    OidSeq os;
    ScanKeyEntryData fkey[2];

    /* try to avoid advancing the seek pointer */
    if (f->f_seqno == (loc / FBLKSIZ)) {
	f->f_offset = loc % FBLKSIZ;
	return (loc);
    }

    /* oh well */
    f->f_seqno = loc / FBLKSIZ;
    f->f_offset = loc % FBLKSIZ;
    (void) index_endscan(f->f_index);

    /* initialize the scan key */
    if (f->f_oslow != (OidSeq) NULL)
	pfree(f->f_oslow);
    if (f->f_oshigh != (OidSeq) NULL)
	pfree(f->f_oshigh);
    f->f_oslow = (OidSeq) mkoidseq(f->f_oid, f->f_seqno);
    ScanKeyEntryInitialize(&fkey[0], 0x0, 1, OidSeqGEProcedure,
			   PointerGetDatum(f->f_oslow));
    f->f_oshigh = (OidSeq) mkoidseq(f->f_oid + 1, 0);
    ScanKeyEntryInitialize(&fkey[1], 0x0, 1, OidSeqLTProcedure,
			   PointerGetDatum(f->f_oshigh));

    /* reinit the index scan */
    f->f_iscan = index_beginscan(f->f_index, 0x0, 2, &fkey[0]);

    /* clear out the heap tid */
    (void) bzero((char *) &(f->f_htid), sizeof(f->f_htid));

    /* done */
    return (loc);
}

int
f262read(f, dbuf, nbytes)
    f262desc *f;
    char *dbuf;
    int nbytes;
{
    int nread;
    HeapTuple htup;
    Buffer b;
    Datum d;
    RetrieveIndexResult res;
    char n;
    int fbytes;
    struct varlena *fdata;

    if (!ItemPointerIsValid(&(f->f_htid))) {
	res = index_getnext(f->f_iscan, ForwardScanDirection);
	if (res != (RetrieveIndexResult) NULL) {
	    bcopy(&(res->heapItemData), &(f->f_htid), sizeof(f->f_htid));
	    pfree(res);
	} else {
	    return (0);
	}
    } else if (f->f_offset >= FBLKSIZ) {
	f->f_offset -= FBLKSIZ;
	f->f_seqno++;
	res = index_getnext(f->f_iscan, ForwardScanDirection);
	if (res != (RetrieveIndexResult) NULL) {
	    bcopy(&(res->heapItemData), &(f->f_htid), sizeof(f->f_htid));
	    pfree(res);
	} else {
	    return (0);
	}
    }

    nread = 0;
    while (nbytes > 0) {
	htup = heap_fetch(f->f_heap, NowTimeQual, &(f->f_htid), &b);
	if (htup == (HeapTuple) NULL) {
	    ReleaseBuffer(b);
	    return (nread);
	}
	d = (Datum) heap_getattr(htup, b, 3, f->f_hdesc, &n);
	fdata = (struct varlena *) DatumGetPointer(d);
	fbytes = MIN((fdata->vl_len - sizeof(fdata->vl_len)), nbytes);
	bcopy(VARDATA(fdata), dbuf, fbytes);
	ReleaseBuffer(b);
	dbuf += fbytes;
	nread += fbytes;
	nbytes -= fbytes;
	if (nbytes > 0) {
	    f->f_offset = 0;
	    f->f_seqno++;
	    res = index_getnext(f->f_iscan, ForwardScanDirection);
	    if (res != (RetrieveIndexResult) NULL) {
		bcopy(&(res->heapItemData), &(f->f_htid), sizeof(f->f_htid));
		pfree(res);
	    } else {
		bzero(&(f->f_htid), sizeof(f->f_htid));
		nbytes = 0;
	    }
	}
    }

    return (nread);
}

int
f262close(f)
    f262desc *f;
{
    index_endscan(f->f_iscan);
    index_close(f->f_index);
    heap_close(f->f_heap);

    if (f->f_oslow != (OidSeq) NULL)
	pfree(f->f_oslow);
    if (f->f_oshigh != (OidSeq) NULL)
	pfree(f->f_oshigh);

    pfree(f);

    return (0);
}

int32
fexport(name, foid)
    struct varlena *name;
    ObjectId foid;
{
    File vfd;
    int nread, nbytes;
    Buffer b;
    HeapTuple htup;
    Datum d;
    char n;
    struct varlena *fdata;
    f262desc *f;
    RetrieveIndexResult res;
    int fbytes;
    ScanKeyEntryData rkey[1];

    /* open the data file */
    if ((vfd = f262file(name, O_WRONLY|O_CREAT|O_TRUNC, 0666)) < 0)
	return (-1);

    /* start at beginning of file */
    FileSeek(vfd, 0L, L_SET);

    f = f262open(foid);

    nread = 0;
    for (;;) {
	res = index_getnext(f->f_iscan, ForwardScanDirection);
	if (res == (RetrieveIndexResult) NULL)
	    break;

	htup = heap_fetch(f->f_heap, NowTimeQual, &(res->heapItemData), &b);
	pfree(res);

	if (htup == (HeapTuple) NULL)
	    continue;

	d = (Datum) heap_getattr(htup, b, 3, f->f_hdesc, &n);
	fdata = (struct varlena *) DatumGetPointer(d);
	fbytes = fdata->vl_len - sizeof(fdata->vl_len);
	if (FileWrite(vfd, VARDATA(fdata), fbytes) < fbytes) {
	    elog(NOTICE, "fexport() write failed");
	    (void) f262close(f);
	    (void) FileClose(vfd);
	    return (nread);
	}

	ReleaseBuffer(b);
	nread += fbytes;
    }

    (void) f262close(f);
    (void) FileClose(vfd);

    return (nread);
}

int32
fabstract(name, foid, blksize, offset, size)
    struct varlena *name;
    ObjectId foid;
    int32 blksize;
    int32 offset;
    int32 size;
{
    File vfd;
    char *buf;
    f262desc *f;
    int blkno;
    int nbytes;

    /* open the disk file for writing */
    if (vfd = f262file(name, O_WRONLY|O_CREAT|O_TRUNC, 0666) < 0)
	return (0);

    /* allocate a buffer for abstraction chunks */
    buf = (char *) palloc(size);

    /* open the base object for reading */
    f = f262open(foid);

    /* create the abstraction */
    blkno = 0;
    for (blkno = 0; ; blkno++) {

	/* seek to start of next abstraction chunk */
	f262seek(f, (blkno * blksize) + offset);

	/* read it */
	if ((nbytes = f262read(f, buf, size)) < size)
	    break;

	/* write to disk file */
	if (FileWrite(vfd, buf, size) < size) {
	    elog(NOTICE, "fabstract() cannot write to disk file");
	    break;
	}
    }

    /* all done */
    FileClose(vfd);
    f262close(f);

    return (blkno);
}
