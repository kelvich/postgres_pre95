/*
 * inv_api.c - routines for manipulating inversion fs large objects
 *
 * $Header$
 */

/*
 * This file contains the user-level large object application interface
 * routines.
 */

#include <sys/file.h>
#include "tmp/c.h"
#include "tmp/libpq-fs.h"
#include "access/genam.h"
#include "access/heapam.h"
#include "access/relscan.h"
#include "access/tupdesc.h"
#include "access/xact.h"
#include "catalog/pg_naming.h"
#include "catalog/pg_lobj.h"
#include "storage/itemptr.h"
#include "storage/bufpage.h"
#include "storage/page.h"
#include "storage/bufmgr.h"
#include "utils/rel.h"
#include "utils/large_object.h"
#include "utils/log.h"
#include "lib/heap.h"
#include "nodes/pg_lisp.h"

/* a little omniscience is a dangerous thing */
#define Int4GEProcedure	150

/*
 *  Warning, Will Robinson...  In order to pack data into an inversion
 *  file as densely as possible, we violate the class abstraction here.
 *  When we're appending a new tuple to the end of the table, we check
 *  the last page to see how much data we can put on it.  If it's more
 *  than IMINBLK, we write enough to fill the page.  This limits external
 *  fragmentation.  In no case can we write more than IMAXBLK, since
 *  the 8K postgres page size less overhead leaves only this much space
 *  for data.
 */

#define IFREESPC(p)	(PageGetFreeSpace(p) - sizeof(HeapTupleData) - sizeof(struct varlena) - sizeof(int32))
#define IMAXBLK		8092
#define IMINBLK		512

extern LargeObject *NewLargeObject();

HeapTuple	inv_fetchtup();
HeapTuple	inv_newtuple();

/*
 *  inv_create -- create a new large object.
 *
 *	Arguments:
 *	  path -- pathname to object in the filesystem/object namespace.
 *	  flags -- storage manager to use, archive mode, etc.
 *
 *	Returns:
 *	  large object descriptor, appropriately filled in.
 */

LargeObjectDesc *
inv_create(path, flags)
char *path;
int flags;
{
    int file_oid;
    LargeObjectDesc *retval;
    LargeObject *newobj;
    Relation r;
    Relation indr;
    int smgr;
    char archchar;
    TupleDescriptor tupdesc;
    AttributeNumber attNums[1];
    ObjectId classObjectId[1];
    NameData objname;
    NameData indname;
    NameData attname;
    NameData typname;

    /* parse flags */
    smgr = flags & INV_SMGRMASK;
    if (flags & INV_ARCHIVE)
	archchar = 'h';
    else
	archchar = 'n';

    /* be sure this is a genuinely new large object */
    if ((file_oid = FilenameToOID(path)) == InvalidObjectId) {

	/* enter it in system relation */
        file_oid = LOcreatOID(path,0);

	/* come up with some table names */
	sprintf(&(objname.data[0]), "Xinv%d", file_oid);
	sprintf(&(indname.data[0]), "Xinx%d", file_oid);

	newobj = NewLargeObject(&(objname.data[0]), CLASS_FILE);

        /* enter cookie into table */
        LOputOIDandLargeObjDesc(file_oid, Inversion, (struct varlena *)newobj);
    }

    /* this is pretty painful...  want a tuple descriptor */
    tupdesc = CreateTemplateTupleDesc(2);
    strcpy((char *) &(attname.data[0]), "olastbyte");
    strcpy((char *) &(typname.data[0]), "int4");
    (void) TupleDescInitEntry((TupleDesc) tupdesc, (AttributeNumber) 1,
			      &attname, &typname, 0);
    strcpy((char *) &(attname.data[0]), "odata");
    strcpy((char *) &(typname.data[0]), "bytea");
    (void) TupleDescInitEntry((TupleDesc) tupdesc, (AttributeNumber) 2,
			      &attname, &typname, 0);

    /*
     *  First create the table to hold the inversion large object.  It
     *  will be located on whatever storage manager the user requested.
     */

    (void) heap_create(&(objname.data[0]), (int) archchar, 2, smgr,
		       (struct attribute **) tupdesc);

    /* make the relation visible in this transaction */
    CommandCounterIncrement();
    r = heap_openr(&objname);

    if (!RelationIsValid(r)) {
	elog(WARN, "cannot create %s on %s under inversion",
		   path, smgrout(smgr));
    }

    /*
     *  Now create a btree index on the relation's olastbyte attribute to
     *  make seeks go faster.  The hardwired constants are embarassing
     *  to me, and are symptomatic of the pressure under which this code
     *  was written.
     */

    attNums[0] = 1;
    classObjectId[0] = 426; /* XXX 426 == int4_ops */
    index_create(&objname, &indname, NULL, 403 /* XXX 403 == btree */,
		 1, &attNums[0], &classObjectId[0],
		 0, (Datum) NULL, (LispValue) NULL);

    /* make the index visible in this transaction */
    CommandCounterIncrement();
    indr = index_openr(&indname);

    if (!RelationIsValid(indr)) {
	elog(WARN, "cannot create index for %s on %s under inversion",
		   path, smgrout(smgr));
    }

    retval = (LargeObjectDesc *) palloc(sizeof(LargeObjectDesc));

    retval->ofs.i_fs.heap_r = r;
    retval->ofs.i_fs.index_r = indr;
    retval->ofs.i_fs.iscan = (IndexScanDesc) NULL;
    retval->ofs.i_fs.hdesc = RelationGetTupleDescriptor(r);
    retval->ofs.i_fs.idesc = RelationGetTupleDescriptor(indr);
    retval->ofs.i_fs.offset = retval->ofs.i_fs.lowbyte = retval->ofs.i_fs.hibyte = 0;
    ItemPointerSetInvalid(&(retval->ofs.i_fs.htid));

    if (flags & INV_WRITE) {
	RelationSetLockForWrite(r);
	retval->ofs.i_fs.flags = IFS_WRLOCK|IFS_RDLOCK;
    } else if (flags & INV_READ) {
	RelationSetLockForRead(r);
	retval->ofs.i_fs.flags = IFS_RDLOCK;
    }
    retval->ofs.i_fs.flags |= IFS_ATEOF;

    retval->object = newobj;

    return(retval);
}

LargeObjectDesc *
inv_open(object, flags)
    LargeObject *object;
    int flags;
{
    LargeObjectDesc *retval;
    Relation r;
    Relation indrel;

    Assert(PointerIsValid(object));
    Assert(object->lo_storage_type == CLASS_FILE);

    r = heap_openr(object->lo_ptr.object_relname);

    if (!RelationIsValid(r))
	return ((LargeObjectDesc *) NULL);

    /*
     *  hack hack hack...  we know that the fourth character of the relation
     *  name is a 'v', and that the fourth character of the index name is an
     *  'x', and that they're otherwise identical.
     */

    object->lo_ptr.object_relname[3] = 'x';
    indrel = index_openr((Name) object->lo_ptr.object_relname);

    if (!RelationIsValid(indrel))
	return ((LargeObjectDesc *) NULL);

    /* put it back */
    object->lo_ptr.object_relname[3] = 'v';

    retval = (LargeObjectDesc *) palloc(sizeof(LargeObjectDesc));

    retval->ofs.i_fs.heap_r = r;
    retval->ofs.i_fs.index_r = indrel;
    retval->ofs.i_fs.iscan = (IndexScanDesc) NULL;
    retval->ofs.i_fs.hdesc = RelationGetTupleDescriptor(r);
    retval->ofs.i_fs.idesc = RelationGetTupleDescriptor(indrel);
    retval->ofs.i_fs.offset = retval->ofs.i_fs.lowbyte = retval->ofs.i_fs.hibyte = 0;
    ItemPointerSetInvalid(&(retval->ofs.i_fs.htid));

    if (flags & INV_WRITE) {
	RelationSetLockForWrite(r);
	retval->ofs.i_fs.flags = IFS_WRLOCK|IFS_RDLOCK;
    } else if (flags & INV_READ) {
	RelationSetLockForRead(r);
	retval->ofs.i_fs.flags = IFS_RDLOCK;
    }

    retval->object = object;

    return(retval);
}

/*
 * Closes an existing large object descriptor.
 */

void
inv_close(obj_desc)
    LargeObjectDesc *obj_desc;
{
    Assert(PointerIsValid(obj_desc));
    Assert(PointerIsValid(obj_desc->object));
    Assert(obj_desc->object->lo_storage_type == CLASS_FILE);

    if (obj_desc->ofs.i_fs.iscan != (IndexScanDesc) NULL)
	index_endscan(obj_desc->ofs.i_fs.iscan);

    heap_close(obj_desc->ofs.i_fs.heap_r);
    index_close(obj_desc->ofs.i_fs.index_r);

    pfree(obj_desc);
}

/*
 * Destroys an existing large object, and frees its associated pointers.
 */

void
inv_destroy(object)
    LargeObject *object;
{
    Assert(PointerIsValid(object));
    Assert(object->lo_storage_type == CLASS_FILE);

    /* XXX not implemented */
    pfree(object);
}

int
inv_stat(obj_desc, stbuf)
    LargeObjectDesc *obj_desc;
    struct pgstat *stbuf;
{
    int ret;

    Assert(PointerIsValid(obj_desc));
    Assert(PointerIsValid(obj_desc->object));
    Assert(obj_desc->object->lo_storage_type == CLASS_FILE);
    Assert(stbuf != NULL);

    /* need read lock for stat */
    if (!(obj_desc->ofs.i_fs.flags & IFS_RDLOCK)) {
	RelationSetLockForRead(obj_desc->ofs.i_fs.heap_r);
	obj_desc->ofs.i_fs.flags |= IFS_RDLOCK;
    }

    return (-1);
}

int
inv_seek(obj_desc, offset, whence)
    LargeObjectDesc *obj_desc;
    int offset;
    int whence;
{
    int ret;
    Datum d;
    ScanKeyEntryData skey[1];

    Assert(PointerIsValid(obj_desc));
    Assert(PointerIsValid(obj_desc->object));
    Assert(obj_desc->object->lo_storage_type == CLASS_FILE);

    /*
     *  Whenever we do a seek, we turn off the EOF flag bit to force
     *  ourselves to check for real on the next read.
     */

    obj_desc->ofs.i_fs.flags &= ~IFS_ATEOF;
    obj_desc->ofs.i_fs.offset = offset;

    /* try to avoid doing any work, if we can manage it */
    if (offset >= obj_desc->ofs.i_fs.lowbyte
	&& offset <= obj_desc->ofs.i_fs.hibyte
	&& obj_desc->ofs.i_fs.iscan != (IndexScanDesc) NULL)
	 return (offset);

    /*
     *  To do a seek on an inversion file, we start an index scan that
     *  will bring us to the right place.  Each tuple in an inversion file
     *  stores the offset of the last byte that appears on it, and we have
     *  an index on this.
     */


    /* right now, just assume that the operation is L_SET */
    if (obj_desc->ofs.i_fs.iscan != (IndexScanDesc) NULL) {
	d = Int32GetDatum(offset);
	btmovescan(obj_desc->ofs.i_fs.iscan, d);
    } else {

	ScanKeyEntryInitialize(&skey[0], 0x0, 1, Int4GEProcedure,
			       Int32GetDatum(offset));

	obj_desc->ofs.i_fs.iscan = index_beginscan(obj_desc->ofs.i_fs.index_r,
					       (Boolean) 0, (uint16) 1,
					       (ScanKey) &skey[0]);
    }

    return (offset);
}

int
inv_tell(obj_desc)
    LargeObjectDesc *obj_desc;
{
    Assert(PointerIsValid(obj_desc));
    Assert(PointerIsValid(obj_desc->object));
    Assert(obj_desc->object->lo_storage_type == CLASS_FILE);

    return (obj_desc->ofs.i_fs.offset);
}

int
inv_read(obj_desc, buf, nbytes)
     LargeObjectDesc *obj_desc;
     char *buf;
     int nbytes;
{
    HeapTuple htup;
    Buffer b;
    int nread;
    int off;
    int ncopy;
    Datum d;
    struct varlena *fsblock;
    bool isNull;

    Assert(PointerIsValid(obj_desc));
    Assert(PointerIsValid(obj_desc->object));
    Assert(obj_desc->object->lo_storage_type == CLASS_FILE);
    Assert(buf != NULL);

    /* if we're already at EOF, we don't need to do any work here */
    if (obj_desc->ofs.i_fs.flags & IFS_ATEOF)
	return (0);

    /* make sure we obey two-phase locking */
    if (!(obj_desc->ofs.i_fs.flags & IFS_RDLOCK)) {
	RelationSetLockForRead(obj_desc->ofs.i_fs.heap_r);
	obj_desc->ofs.i_fs.flags |= IFS_RDLOCK;
    }

    nread = 0;

    /* fetch a block at a time */
    while (nread < nbytes) {

	/* fetch an inversion file system block */
	htup = inv_fetchtup(obj_desc, &b);

	if (!HeapTupleIsValid(htup)) {
	    obj_desc->ofs.i_fs.flags |= IFS_ATEOF;
	    break;
	}

	/* copy the data from this block into the buffer */
	d = (Datum) heap_getattr(htup, b, 2, obj_desc->ofs.i_fs.hdesc, &isNull);
	fsblock = (struct varlena *) DatumGetPointer(d);

	off = obj_desc->ofs.i_fs.offset - obj_desc->ofs.i_fs.lowbyte;
	ncopy = obj_desc->ofs.i_fs.hibyte - obj_desc->ofs.i_fs.offset + 1;
	bcopy(&(fsblock->vl_dat[0]), buf, ncopy);

	/* be a good citizen */
	ReleaseBuffer(b);

	/* move pointers past the amount we just read */
	buf += ncopy;
	nread += ncopy;
	obj_desc->ofs.i_fs.offset += ncopy;
    }

    /* that's it */
    return (nread);
}

int
inv_write(obj_desc, buf, nbytes)
     LargeObjectDesc *obj_desc;
     char *buf;
     int nbytes;
{
    HeapTuple htup;
    Buffer b;
    int nwritten;
    int tuplen;

    Assert(PointerIsValid(obj_desc));
    Assert(PointerIsValid(obj_desc->object));
    Assert(obj_desc->object->lo_storage_type == CLASS_FILE);
    Assert(buf != NULL);

    /*
     *  Make sure we obey two-phase locking.  A write lock entitles you
     *  to read the relation, as well.
     */

    if (!(obj_desc->ofs.i_fs.flags & IFS_WRLOCK)) {
	RelationSetLockForRead(obj_desc->ofs.i_fs.heap_r);
	obj_desc->ofs.i_fs.flags |= (IFS_WRLOCK|IFS_RDLOCK);
    }

    nwritten = 0;

    /* write a block at a time */
    while (nwritten < nbytes) {

	/*
	 *  Fetch the current inverstion file system block.  If the
	 *  class storing the inversion file is empty, we don't want
	 *  to do an index lookup, since index lookups choke on empty
	 *  files (should be fixed someday).
	 */

	if ((obj_desc->ofs.i_fs.flags & IFS_ATEOF)
	    || obj_desc->ofs.i_fs.heap_r->rd_nblocks == 0)
	    htup = (HeapTuple) NULL;
	else
	    htup = inv_fetchtup(obj_desc, &b);

	/* either append or replace a block, as required */
	if (!HeapTupleIsValid(htup)) {
	    tuplen = inv_wrnew(obj_desc, buf, nbytes - nwritten);
	} else {
	    tuplen = inv_wrold(obj_desc, buf, nbytes - nwritten, htup, b);
	}

	/* move pointers past the amount we just wrote */
	buf += tuplen;
	nwritten += tuplen;
	obj_desc->ofs.i_fs.offset += tuplen;
    }

    /* that's it */
    return (nwritten);
}

/*
 *  inv_fetchtup -- Fetch an inversion file system block.
 *
 *	This routine finds the file system block containing the offset
 *	recorded in the obj_desc structure.  Later, we need to think about
 *	the effects of non-functional updates (can you rewrite the same
 *	block twice in a single transaction?), but for now, we won't bother.
 *
 *	Parameters:
 *		obj_desc -- the object descriptor.
 *		bufP -- pointer to a buffer in the buffer cache; caller
 *		        must free this.
 *
 *	Returns:
 *		A heap tuple containing the desired block, or NULL if no
 *		such tuple exists.
 */

HeapTuple
inv_fetchtup(obj_desc, bufP)
    LargeObjectDesc *obj_desc;
    Buffer *bufP;
{
    HeapTuple htup;
    IndexTuple itup;
    RetrieveIndexResult res;
    Datum d;
    int firstbyte, lastbyte;
    struct varlena *fsblock;
    bool isNull;

    /*
     *  If we've exhausted the current block, we need to get the next one.
     *  When we support time travel and non-functional updates, we will
     *  need to loop over the blocks, rather than just have an 'if', in
     *  order to find the one we're really interested in.
     */

    if (obj_desc->ofs.i_fs.offset > obj_desc->ofs.i_fs.hibyte
	|| obj_desc->ofs.i_fs.offset < obj_desc->ofs.i_fs.lowbyte
	|| !ItemPointerIsValid(&(obj_desc->ofs.i_fs.htid))) {

	do {
	    res = index_getnext(obj_desc->ofs.i_fs.iscan, ForwardScanDirection);

	    if (res == (RetrieveIndexResult) NULL) {
		ItemPointerSetInvalid(&(obj_desc->ofs.i_fs.htid));
		return ((HeapTuple) NULL);
	    }

	    /*
	     *  For time travel, we need to use the actual time qual here,
	     *  rather that NowTimeQual.  We currently have no way to pass
	     *  a time qual in.
	     */

	    htup = heap_fetch(obj_desc->ofs.i_fs.heap_r, NowTimeQual,
			      &(res->heapItemData), bufP);

	} while (htup == (HeapTuple) NULL);

	/* remember this tid -- we may need it for later reads/writes */
	ItemPointerCopy(&(res->heapItemData), &(obj_desc->ofs.i_fs.htid));

    } else {
	htup = heap_fetch(obj_desc->ofs.i_fs.heap_r, NowTimeQual,
		          &(obj_desc->ofs.i_fs.htid), bufP);
    }

    /*
     *  By here, we have the heap tuple we're interested in.  We cache
     *  the upper and lower bounds for this block in the object descriptor
     *  and return the tuple.
     */

    d = (Datum)heap_getattr(htup, *bufP, 1, obj_desc->ofs.i_fs.hdesc, &isNull);
    lastbyte = (int32) DatumGetInt32(d);
    d = (Datum)heap_getattr(htup, *bufP, 2, obj_desc->ofs.i_fs.hdesc, &isNull);
    fsblock = (struct varlena *) DatumGetPointer(d);
    firstbyte = lastbyte - fsblock->vl_len - sizeof(fsblock->vl_len);

    obj_desc->ofs.i_fs.lowbyte = firstbyte;
    obj_desc->ofs.i_fs.hibyte = lastbyte;

    /* done */
    return (htup);
}

/*
 *  inv_wrnew() -- append a new filesystem block tuple to the inversion
 *		    file.
 *
 *	In response to an inv_write, we append one or more file system
 *	blocks to the class containing the large object.  We violate the
 *	class abstraction here in order to pack things as densely as we
 *	are able.  We examine the last page in the relation, and write
 *	just enough to fill it, assuming that it has above a certain
 *	threshold of space available.  If the space available is less than
 *	the threshold, we allocate a new page by writing a big tuple.
 *
 *	By the time we get here, we know all the parameters passed in
 *	are valid, and that we hold the appropriate lock on the heap
 *	relation.
 *
 *	Parameters:
 *		obj_desc: large object descriptor for which to append block.
 *		buf: buffer containing data to write.
 *		nbytes: amount to write
 *
 *	Returns:
 *		number of bytes actually written to the new tuple.
 */

int
inv_wrnew(obj_desc, buf, nbytes)
    LargeObjectDesc *obj_desc;
    char *buf;
    int nbytes;
{
    Relation hr;
    HeapTuple ntup;
    Buffer buffer;
    Page page;
    int nblocks;
    int nwritten;

    hr = obj_desc->ofs.i_fs.heap_r;

    /* get the last block in the relation */
    nblocks = RelationGetNumberOfBlocks(hr);
    buffer = ReadBuffer(hr, nblocks);
    page = BufferSimpleGetPage(buffer);

    /*
     *  If the last page is too small to hold all the data, and it's too
     *  small to hold IMINBLK, then we allocate a new page.  If it will
     *  hold at least IMINBLK, but less than all the data requested, then
     *  we write IMINBLK here.  The caller is responsible for noticing that
     *  less than the requested number of bytes were written, and calling
     *  this routine again.
     */

    nwritten = IFREESPC(page);
    if (nwritten < nbytes) {
	if (nwritten < IMINBLK) {
            ReleaseBuffer(buffer);
            buffer = ReadBuffer(hr, P_NEW);
            page = BufferSimpleGetPage(buffer);
            BufferSimpleInitPage(buffer);
	    if (nwritten > IMAXBLK)
		nwritten = IMAXBLK;
	    else
		nwritten = nbytes;
	}
    } else {
	nwritten = nbytes;
    }

    /*
     *  Insert a new file system block tuple, index it, and write it out.
     */

    ntup = inv_newtuple(obj_desc, buffer, page, buf, nwritten);
    inv_indextup(obj_desc, ntup);

    /* new tuple is inserted */
    WriteBuffer(buffer);

    return (nwritten);
}

int
inv_wrold(obj_desc, dbuf, nbytes, htup, buffer)
    LargeObjectDesc *obj_desc;
    char *dbuf;
    int nbytes;
    HeapTuple htup;
    Buffer buffer;
{
    Relation hr;
    HeapTuple ntup;
    Buffer newbuf;
    Page page;
    Page newpage;
    PageHeader ph;
    int tupbytes;
    Datum d;
    struct varlena *fsblock;
    int nwritten, nblocks, freespc;
    int loc, sz;
    char *dptr;
    bool isNull;

    /*
     *  Since we're using a no-overwrite storage manager, the way we
     *  overwrite blocks is to mark the old block invalid and append
     *  a new block.  First mark the old block invalid.  This violates
     *  the tuple abstraction.
     */

    TransactionIdStore(GetCurrentTransactionId(), (Pointer)htup->t_xmax);
    htup->t_cmax = GetCurrentCommandId();

    /*
     *  If we're overwriting the entire block, we're lucky.  All we need
     *  to do is to insert a new block.
     */

    if (obj_desc->ofs.i_fs.offset == obj_desc->ofs.i_fs.lowbyte
	&& obj_desc->ofs.i_fs.lowbyte + nbytes >= obj_desc->ofs.i_fs.hibyte) {
	WriteBuffer(buffer);
	return (inv_wrnew(obj_desc, dbuf, nbytes));
    }

    /*
     *  By here, we need to overwrite part of the data in the current
     *  tuple.  In order to reduce the degree to which we fragment blocks,
     *  we guarantee that no block will be broken up due to an overwrite.
     *  This means that we need to allocate a tuple on a new page, if
     *  there's not room for the replacement on this one.
     */

    newbuf = buffer;
    newpage = page;
    hr = obj_desc->ofs.i_fs.heap_r;
    freespc = IFREESPC(page);
    d = (Datum)heap_getattr(htup, buffer, 2, obj_desc->ofs.i_fs.hdesc, &isNull);
    fsblock = (struct varlena *) DatumGetPointer(d);
    tupbytes = fsblock->vl_len - sizeof(fsblock->vl_len);

    if (freespc < tupbytes) {

	/*
	 *  First see if there's enough space on the last page of the
	 *  table to put this tuple.
	 */

	nblocks = RelationGetNumberOfBlocks(hr);
	newbuf = ReadBuffer(hr, nblocks);
        newpage = BufferSimpleGetPage(newbuf);
	freespc = IFREESPC(newpage);

	/*
	 *  If there's no room on the last page, allocate a new last
	 *  page for the table, and put it there.
	 */

	if (freespc < tupbytes) {
	    ReleaseBuffer(newbuf);
	    newbuf = ReadBuffer(hr, P_NEW);
	    newpage = BufferSimpleGetPage(newbuf);
	    BufferSimpleInitPage(newbuf);
	}
    }

    /*
     *  By here, we have a page (newpage) that's guaranteed to have
     *  enough space on it to put the new tuple.  Call inv_newtuple
     *  to do the work.  Passing NULL as a buffer to inv_newtuple()
     *  keeps it from copying any data into the new tuple.  When it
     *  returns, the tuple is ready to receive data from the old
     *  tuple and the user's data buffer.
     */

    ntup = inv_newtuple(obj_desc, newbuf, newpage, (char *) NULL, tupbytes);
    dptr = ((char *) ntup) + ntup->t_hoff - sizeof(ntup->t_bits) + sizeof(int4)
		+ sizeof(fsblock->vl_len);

    if (obj_desc->ofs.i_fs.offset > obj_desc->ofs.i_fs.lowbyte) {
	bcopy(&(fsblock->vl_dat[0]), dptr,
		obj_desc->ofs.i_fs.offset - obj_desc->ofs.i_fs.lowbyte);
	dptr += obj_desc->ofs.i_fs.offset - obj_desc->ofs.i_fs.lowbyte;
    }

    nwritten = nbytes;
    if (nwritten > obj_desc->ofs.i_fs.hibyte - obj_desc->ofs.i_fs.offset)
	nwritten = obj_desc->ofs.i_fs.hibyte - obj_desc->ofs.i_fs.offset;

    bcopy(dbuf, dptr, nwritten);
    dptr += nwritten;

    if (obj_desc->ofs.i_fs.offset + nwritten < obj_desc->ofs.i_fs.hibyte) {
	loc = (obj_desc->ofs.i_fs.hibyte - obj_desc->ofs.i_fs.offset)
		+ nwritten;
	sz = obj_desc->ofs.i_fs.hibyte - (obj_desc->ofs.i_fs.lowbyte + loc);
	bcopy(dptr, &(fsblock->vl_dat[0]), sz);
    }

    /* index the new tuple */
    inv_indextup(obj_desc, ntup);

    /*
     *  Okay, by here, a tuple for the new block is correctly placed,
     *  indexed, and filled.  Write the changed pages out.
     */

    WriteBuffer(buffer);
    if (newbuf != buffer)
	WriteBuffer(newbuf);

    /* done */
    return (nwritten);
}

HeapTuple
inv_newtuple(obj_desc, buffer, page, dbuf, nwrite)
    LargeObjectDesc *obj_desc;
    Buffer buffer;
    Page page;
    char *dbuf;
    int nwrite;
{
    HeapTuple ntup;
    PageHeader ph;
    int tupsize;
    int hoff;
    register i;
    Offset lower;
    Offset upper;
    ItemId itemId;
    OffsetNumber off;
    OffsetNumber limit;
    char *attptr;
    
    /* compute tuple size -- no nulls */
    hoff = sizeof(HeapTupleData) - sizeof(ntup->t_bits);

    /* add in olastbyte, varlena.vl_len, varlena.vl_dat */
    tupsize = hoff + (3 * sizeof(int32)) + nwrite;
    tupsize = LONGALIGN(tupsize);

    /*
     *  Allocate the tuple on the page, violating the page abstraction.
     *  This code was swiped from PageAddItem().
     */

    ph = (PageHeader) page;
    limit = 2 + PageGetMaxOffsetIndex(page);

    /* look for "recyclable" (unused & deallocated) ItemId */
    for (off = 1; off < limit; off++) {
	itemId = &ph->pd_linp[off - 1];
	if ((((*itemId).lp_flags & LP_USED) == 0) && 
	    ((*itemId).lp_len == 0)) 
	    break;
    }

    if (off > limit)
	lower = (Offset) (((char *) (&ph->pd_linp[off])) - ((char *) page));
    else if (off == limit)
	lower = ph->pd_lower + sizeof (ItemIdData);
    else
	lower = ph->pd_lower;

    upper = ph->pd_upper - tupsize;
    
    itemId = &ph->pd_linp[off - 1];
    (*itemId).lp_off = upper;
    (*itemId).lp_len = tupsize;
    (*itemId).lp_flags = LP_USED;
    ph->pd_lower = lower;
    ph->pd_upper = upper;

    ntup = (HeapTuple) ((char *) page + upper);

    /*
     *  Tuple is now allocated on the page.  Next, fill in the tuple
     *  header.  This block of code violates the tuple abstraction.
     */

    ntup->t_len = tupsize;
    ItemPointerSet(&(ntup->t_ctid), 0, BufferGetBlockNumber(buffer), 0, off);
    ItemPointerSetInvalid(&(ntup->t_chain));
    ItemPointerSetInvalid(&(ntup->t_lock.l_ltid));
    LastOidProcessed = ntup->t_oid = newoid();
    TransactionIdStore(GetCurrentTransactionId(), (Pointer)ntup->t_xmin);
    ntup->t_cmin = GetCurrentCommandId();
    PointerStoreInvalidTransactionId((Pointer)ntup->t_xmax);
    ntup->t_cmax = 0;
    ntup->t_tmin = ntup->t_tmax = InvalidTime;
    ntup->t_natts = 2;
    ntup->t_locktype = 'd';
    ntup->t_hoff = hoff;
    ntup->t_vtype = 'r';
    ntup->t_infomask = 0x0;

    /*
     *  Finally, copy the user's data buffer into the tuple.  This violates
     *  the tuple and class abstractions.
     */

    attptr = ((char *) ntup) + hoff;
    *((int32 *) attptr) = obj_desc->ofs.i_fs.offset + nwrite - 1;
    attptr += sizeof(int32);

    /*
    **	mer fixed disk layout of varlenas to get rid of the need for this.
    **
    **	*((int32 *) attptr) = nwrite + sizeof(int32);
    **	attptr += sizeof(int32);
    */

    *((int32 *) attptr) = nwrite + sizeof(int32);
    attptr += sizeof(int32);

    /*
     *  If a data buffer was passed in, then copy the data from the buffer
     *  to the tuple.  Some callers (eg, inv_wrold()) may not pass in a
     *  buffer, since they have to copy part of the old tuple data and
     *  part of the user's new data into the new tuple.
     */

    if (dbuf != (char *) NULL)
	bcopy(dbuf, attptr, nwrite);

    /* keep track of boundary of current tuple */
    obj_desc->ofs.i_fs.lowbyte = obj_desc->ofs.i_fs.offset;
    obj_desc->ofs.i_fs.hibyte = obj_desc->ofs.i_fs.offset + nwrite - 1;

    /* new tuple is filled -- return it */
    return (ntup);
}

inv_indextup(obj_desc, htup)
    LargeObjectDesc *obj_desc;
    HeapTuple htup;
{
    IndexTuple itup;
    GeneralInsertIndexResult res;
    Datum v[1];
    char n[1];

    n[0] = ' ';
    v[0] = Int32GetDatum(obj_desc->ofs.i_fs.hibyte);
    itup = index_formtuple(1, obj_desc->ofs.i_fs.idesc, &v[0], &n[0]);
    bcopy(&(htup->t_ctid), &(itup->t_tid), sizeof(ItemPointerData));
    res = index_insert(obj_desc->ofs.i_fs.index_r, itup, (double *) NULL);

    if (res)
	pfree ((char *) res);

    pfree ((char *) itup);
}

inv_showheap(obj_desc)
    LargeObjectDesc *obj_desc;
{
    Buffer b;
    Page p;
    int nblocks;
    int i;

    nblocks = RelationGetNumberOfBlocks(obj_desc->ofs.i_fs.heap_r);

    for (i = 0; i < nblocks; i++) {
	b = ReadBuffer(obj_desc->ofs.i_fs.heap_r, i);
	p = BufferSimpleGetPage(b);
	DumpPage(p, i);
	ReleaseBuffer(b);
    }
}

extern String ItemPointerFormExternal ARGS((ItemPointer pointer ));

DumpPage(page, blkno)
	Page	page;
	int blkno;
{
	ItemId		lp;
	HeapTuple	tup;
	int		flags, i, nline;
	ItemPointerData	pointerData;

	printf("\t[subblock=%d]:lower=%d:upper=%d:special=%d\n", 0,
		((PageHeader)page)->pd_lower, ((PageHeader)page)->pd_upper,
		((PageHeader)page)->pd_special);

	printf("\t:MaxOffsetIndex=%d:InternalFragmentation=%d\n",
		(int16)PageGetMaxOffsetIndex(page),
		PageGetInternalFragmentation(page));

	nline = 1 + (int16)PageGetMaxOffsetIndex(page);

	/* add printing of the specially allocated fields */
{
	int	i;
	char	*cp;

	i = PageGetSpecialSize(page);
	cp = PageGetSpecialPointer(page);

	printf("\t:SpecialData=");

	while (i > 0) {
		printf(" 0x%02x", *cp);
		cp += 1;
		i -= 1;
	}
	printf("\n");
}
	for (i = 0; i < nline; i++) {
		lp = ((PageHeader)page)->pd_linp + i;
		flags = (*lp).lp_flags;
		ItemPointerSet(&pointerData, 0, blkno, 0, 1 + i);
		printf("%s:off=%d:flags=0x%x:len=%d",
			ItemPointerFormExternal(&pointerData), (*lp).lp_off,
			flags, (*lp).lp_len);
		if (flags & LP_USED)
			printf(":USED");
		if (flags & LP_IVALID)
			printf(":IVALID");
		if (flags & LP_DOCNT) {
			ItemPointer	pointer;

			pointer = (ItemPointer)(uint16 *)
				((char *)page + (*lp).lp_off);
			
			printf(":DOCNT@%s", ItemPointerFormExternal(pointer));
		}
		if (flags & LP_CTUP)
			printf(":CTUP");
		if (flags & LP_LOCK)
			printf(":LOCK");

		if (flags & LP_USED) {

			HeapTupleData	htdata;

			bcopy((char *) &((char *)page)[(*lp).lp_off],
				(char *) &htdata, sizeof(htdata));

			tup = &htdata;

			if (flags & LP_DOCNT) {
				bcopy((char *) &((char *)tup)[TCONTPAGELEN],
					(char *) &htdata, sizeof(tup));
			}

			printf("\n\t:ctid=%s:oid=%ld",
				ItemPointerFormExternal(&tup->t_ctid),
				tup->t_oid);
			printf(":natts=%d:thoff=%d:vtype=`%c' (0x%02x):",
				tup->t_natts,
				tup->t_hoff, tup->t_vtype, tup->t_vtype);

			printf("\n\t:tmin=%d:cmin=%u:",
				tup->t_tmin, tup->t_cmin);
			printf("xmin=0x%02x%02x%02x%02x%02x:",
				(unsigned char) tup->t_xmin[0],
				(unsigned char) tup->t_xmin[1],
				(unsigned char) tup->t_xmin[2],
				(unsigned char) tup->t_xmin[3],
				(unsigned char) tup->t_xmin[4]);

			printf("\n\t:tmax=%d:cmax=%u:",
				tup->t_tmax, tup->t_cmax);
			printf("xmax=0x%02x%02x%02x%02x%02x:",
				(unsigned char) tup->t_xmax[0],
				(unsigned char) tup->t_xmax[1],
				(unsigned char) tup->t_xmax[2],
				(unsigned char) tup->t_xmax[3],
				(unsigned char) tup->t_xmax[4]);

			printf("\n\t:chain=%s:\n",
				ItemPointerFormExternal(&tup->t_chain));
		} else
			putchar('\n');
	}
}

String
ItemPointerFormExternal(pointer)
	ItemPointer	pointer;
{
	static char	itemPointerString[32];

	if (!ItemPointerIsValid(pointer)) {
		bcopy("<-,-,->", itemPointerString, sizeof "<-,-,->");
	} else {
		sprintf(itemPointerString, "<%lu,%u,%u>",
			ItemPointerGetBlockNumber(pointer),
			ItemPointerSimpleGetPageNumber(pointer),
			ItemPointerSimpleGetOffsetNumber(pointer));
	}

	return (itemPointerString);
}
