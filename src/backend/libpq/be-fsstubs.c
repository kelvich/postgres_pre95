/*#define FSDB 1*/
/* ----------------------------------------------------------------
 *   FILE
 *	be-fsstubs.c
 *	
 *   DESCRIPTION
 *	support for filesystem operations on large objects
 *
 *   SUPPORT ROUTINES
 *
 *   INTERFACE ROUTINES
 * int LOopen(char *fname, int mode);
 * int LOclose(int fd);
 * bytea LOread(int fd, int len);
 * int LOwrite(int fd, char *buf);
 * int LOlseek(int fd,int offset,int whence);
 * int LOcreat(char *path, int mode,int objtype);
 * int LOtell(int fd);
 * int LOftruncate(int fd);
 * bytea LOstat(int fd);
 * int LOmkdir(char *path, int mode);
 * int LOrmdir(char *path);
 * int LOunlink(char *path);
 *	
 *   NOTES
 *      This should be moved to a more appropriate place.  It is here
 *      for lack of a better place.
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

/*
 * Builtin functions for open/close/read/write operations on large objects.
 *
 * The functions of the same name in the ADT implementations of large objects
 * should be made less general, e.g. GregLOOpen, etc.  These stubs are meant 
 * to handle any large object.
 *
 * These functions operate in the current portal variable context, which 
 * means the large object descriptors hang around between transactions and 
 * are not deallocated until explicitly closed, or until the portal is 
 * closed.
 *
 *
 * int LOopen(char *fname, int mode);
 *     return file descriptor that can subsequently be used in LOread/write/
 *     close.
 * int LOclose(int fd);
 * bytea LOread(int fd, int len);
 * int LOwrite(int fd, char *buf);
 * int LOlseek(int fd,int offset,int whence);
 * int LOcreat(char *path, int mode, int objtype);
 * int LOtell(int fd);
 * int LOftruncate(int fd);
 * bytea LOstat(int fd);
 * int LOmkdir(char *path, int mode);
 * int LOrmdir(char *path);
 * int LOunlink(char *path);
 *
 * Implementation:
 *    LOopen given filename and mode (ignored):
 *       map filename to OID of large object (FilenameToOID).
 *       map OID to large object cookie (LOassocOIDandLargeObjDesc).
 *       call LO dependent open on cookie.
 *       allocate a file descriptor in table corresponding to result of open.
 *       return file descriptor.
 *      (if at any step, there is an error, e.g. OID not found, table full,
 *       return an error.)
 *    LOclose given file descriptor
 *       call LO dependent close on cookie.
 *       deallocate file descriptor in table.
 *    LOread given file descriptor, length
 *       map length to object dependent addressing, e.g. bytes to blocks.
 *       call LO dependent read on cookie.
 *       return variable length byte array.
 *    LOwrite given file descriptor, byte array
 *       map length of array to object dependent addressing
 *       call LO dependent write on cookie (possibly reading and re-writing
 *       partial blocks).
 */

#include "tmp/c.h"
#include "tmp/postgres.h"
#include "tmp/simplelists.h"
#include "tmp/libpq.h"
#include "tmp/libpq-fs.h"
/*#include "utils/large_object.h"*/
#include "utils/mcxt.h"
#include "catalog/pg_lobj.h"
#include "catalog/pg_naming.h"

#include "utils/log.h"

static GlobalMemory fscxt = NULL;

/* HAck protos.  move to .h later */
/* lo_api.c */
void *LOCreate ARGS((char *path , int open_mode ));
void *LOOpen ARGS((struct varlena *object , int open_mode ));
void LOClose ARGS((void *obj_desc ));
int LOUnixStat ARGS((void *obj_desc , struct pgstat *stbuf ));
int LOSeek ARGS((void *obj_desc , int offset , int whence ));
int LOTell ARGS((void *obj_desc ));
int LORead ARGS((void *obj_desc , char *buf , int n ));
int LOWrite ARGS((void *obj_desc , char *buf , int n ));

/* ftype.c */
void *inv_create ARGS((char *name, int mode ));
void  *inv_open ARGS((ObjectId foid ));
int inv_read ARGS((void *f , char *dbuf , int nbytes ));
int inv_write ARGS((void *f , char *dbuf , int nbytes ));
void inv_close ARGS((void *f ));
int inv_seek ARGS((void *f , int loc ));

static int noaction() { return  -1; } /* error */
static void *noactionnull() { return (void *) NULL; } /* error */
/* These should be easy to code */
static int inv_seekwhence() {return -1;}
static int inv_tell() {return -1;}
static int inv_unixstat() {return -1;}

/* This is ordered upon objtype values.  Make sure that entries are sorted
   properly. */

static struct {
    int bigcookie; 
#define SMALL_INT 0	 /* is integer wrapped in a varlena */
#define BIG 1  /* must be passed as varlena*/
    void *(*LOcreate) ARGS((char *,int)); /* name,mode -> runtime-cookie */
    void *(*LOopen) ARGS((void *,int)); /* persistant-cookie,mode -> runtime-cookie */
    void (*LOclose) ARGS((void *)); /* runtime-cookie */
    int (*LOread) ARGS((void *,char *,int)); /* rt-cookie,buf,length -> bytecount*/
    int (*LOwrite) ARGS((void *,char *,int)); /*rt-cookie,buf,length -> bytecount*/
    int (*LOseek) ARGS((void *,int,int)); /*rt-cookie,offset,whence -> bytecount */
    int (*LOtell) ARGS((void *)); /*rt-cookie ->bytecount*/
    int (*LOunixstat) ARGS((void *,struct pgstat *)); /* rt-cookie,stat ->errorcode*/
} LOprocs[] = {
    /* Inversion */
    { SMALL_INT,noactionnull, inv_open, inv_close, inv_read, noaction,
	inv_seekwhence, inv_tell, inv_unixstat},
    /* Unix */
    { BIG, LOCreate, LOOpen, LOClose, LORead, LOWrite,
	LOSeek, LOTell, LOUnixStat}
};


int LOopen(fname,mode)
     char *fname;
     int mode;
{
    oid loOID;
    struct varlena *lobjCookie;
    char *lobjDesc;
    int fd;
    int objtype;
    MemoryContext currentContext;

#if FSDB
    elog(NOTICE,"LOopen(%s,%d)",fname,mode);
#endif
    loOID = FilenameToOID(fname);

    if (loOID == InvalidObjectId) { /* lookup failed */
	if ((mode  & O_CREAT) != O_CREAT) {
#if FSDB
	elog(NOTICE,"LOopen lookup failed");
#endif
	    return -1;
	} else {
	    /* Have to choose a particular type */
	    objtype = Unix;
	    return LOcreat(fname,mode,objtype);
	}
    }
    
    lobjCookie = LOassocOIDandLargeObjDesc(&objtype,loOID);
    
    if (lobjCookie == NULL) { /* lookup failed, perhaps a dir? */
#if FSDB
	elog(NOTICE,"LOopen assoc failed");
#endif
	return -1;
    } else {
#if FSDB
	elog(NOTICE,"LOopen assoc succeeded");
#endif
    }
  
    /* temp debug testing */
    {
	if (fscxt == NULL) {
	    fscxt = CreateGlobalMemory("Filesystem");
	}
	currentContext = MemoryContextSwitchTo((MemoryContext)fscxt);
#if 0
	/* switch context to portal cxt */
	{
	    PortalEntry * curPort;
	    curPort = be_currentportal();
	    currentContext = MemoryContextSwitchTo(curPort->portalcxt);
	}
#endif
	switch (LOprocs[objtype].bigcookie) {
	  case SMALL_INT:
	    lobjDesc = (char *)
	      LOprocs[objtype].LOopen((void *) *((int *)VARDATA(lobjCookie)),mode);
	    break;
	  case BIG: 
	    lobjDesc = (char *) LOprocs[objtype].LOopen(lobjCookie,mode);
	    break;
	}
	    
	if (lobjDesc == NULL) {
	    MemoryContextSwitchTo(currentContext);
#if FSDB
	    elog(NOTICE,"LOopen objtype specific open failed");
#endif
	    return -1;
	}
	fd = NewLOfd(lobjDesc,objtype);
	/* switch context back to orig. */
	MemoryContextSwitchTo(currentContext);
    }
    return fd;
}

#define MAX_LOBJ_FDS 256
char  *cookies[MAX_LOBJ_FDS];
int lotype[MAX_LOBJ_FDS];

#if deadcode
static int current_objaddr[MAX_LOBJ_FDS]; /* current address in LO's terms */
static int current_objoffset[MAX_LOBJ_FDS]; /* offset in LO's terms */
#endif

int LOclose(fd)
     int fd;
{
    void DeleteLOfd();

    if (fd >= MAX_LOBJ_FDS) {
	elog(WARN,"LOclose(%d) out of range",fd);
	return -2;
    }
    if (cookies[fd] == NULL) {
	elog(WARN,"LOclose(%d) invalid file descriptor",fd);
	return -3;
    }
#if FSDB
	elog(NOTICE,"LOclose(%d)",fd);
#endif
    /* Kemnitz large objects */
    {
	MemoryContext currentContext;
	Assert(fscxt != NULL);
	currentContext = MemoryContextSwitchTo((MemoryContext)fscxt);
	
	LOprocs[lotype[fd]].LOclose(cookies[fd]);

	MemoryContextSwitchTo(currentContext);
    }
    DeleteLOfd(fd);
    return 0;
}

/*
 *  We assume the large object supports byte oriented reads and seeks so
 *  that our work is easier.
 */
struct varlena *
LOread(fd,len)
     int fd;
     int len;
{
    struct varlena *retval;
    int bytestoread;
    int totalread = 0;

    retval = (struct varlena *)palloc(sizeof(int32) + len);
    totalread = LOprocs[lotype[fd]].LOread(cookies[fd],VARDATA(retval),len);
    VARSIZE(retval) = totalread;

    return retval;
}

int LOwrite(fd,wbuf)
     int fd;
     struct varlena *wbuf;
{
    int totalwritten;
    int bytestowrite;

    bytestowrite = VARSIZE(wbuf);
    totalwritten = LOprocs[lotype[fd]].LOwrite(cookies[fd],VARDATA(wbuf),
					       bytestowrite);
    return totalwritten;
}

int
LOlseek(fd,offset,whence)
     int fd,offset,whence;
{
    int pos;

    if (fd >= MAX_LOBJ_FDS) {
	elog(WARN,"LOSeek(%d) out of range",fd);
	return -2;
    }
    return LOprocs[lotype[fd]].LOseek(cookies[fd],offset,whence);
}

int
LOcreat(path,mode,objtype)
     char *path;
     int mode;
     int objtype;
{
    char *lobjDesc;
    int fd;
    MemoryContext currentContext;

    /* prevent garbage code */
    if (objtype != Inversion && objtype != Unix)
	objtype = Unix;

    if (fscxt == NULL) {
	fscxt = CreateGlobalMemory("Filesystem");
    }

    currentContext = MemoryContextSwitchTo((MemoryContext)fscxt);

    lobjDesc = (char *) LOprocs[objtype].LOcreate(path,mode);

    if (lobjDesc == NULL) {
	MemoryContextSwitchTo(currentContext);
	return -1;
    }

    fd = NewLOfd(lobjDesc,objtype);

    /* switch context back to original memory context */
    MemoryContextSwitchTo(currentContext);

    return fd;
}

int LOtell(fd)
     int fd;
{
    if (fd >= MAX_LOBJ_FDS) {
	elog(WARN,"LOtell(%d) out of range",fd);
	return -2;
    }
    if (cookies[fd] == NULL) {
	elog(WARN,"LOtell(%d) cookie is NULL",fd);
	return -3;
    }
    return LOprocs[lotype[fd]].LOtell(cookies[fd]);
}

int LOftruncate()
{
}

struct varlena *
LOstat(path)
     char *path;
{
    struct varlena *ret;
    struct pgstat *st;
    unsigned int nblocks, byte_offset;
    int len;
    int pathOID;
    len = sizeof(struct pgstat);
    ret = (struct varlena *) palloc(len+sizeof(int32));
    VARSIZE(ret) = len;
    st = (struct pgstat *)VARDATA(ret);
    bzero(st,len);	/* default values of 0 */

    pathOID =  FilenameToOID(path);
    st->st_ino = pathOID;

    if (!LOisdir(path) && pathOID != InvalidObjectId) {
	int fd = LOopen(path,O_RDONLY);
	LOprocs[lotype[fd]].LOunixstat(cookies[fd],st);
	LOclose(fd);
    } else if (pathOID != InvalidObjectId) { /* isdir */
	st->st_mode = S_IFDIR;
	/* our directoiries don't exist in the filesystem, so give them
	   artificial permissions */
	st->st_uid = getuid(); /* fake uid */
	st->st_mode |= S_IRUSR|S_IWUSR|S_IXUSR;
    } else {
	VARSIZE(ret) = 5;
    }

    return ret;
}

/*
 * LOrename can be found in naming.c
 */

int LOmkdir(path,mode)
     char *path;
     int mode;
{
    oid oidf;
    /* enter new pathname */
    oidf = LOcreatOID(path,mode);
    if (oidf == InvalidObjectId) {
	return -1;
    } else {
	return 0;
    }
}

int LOrmdir(path)
     char *path;
{
    if (!LOisdir(path))
      return -1;
    else if (!LOisemptydir(path))
      return -1;
    else {
      LOunlink(path);
      return 0;
    }
}

int LOunlink(path)
     char *path;
{
    /* remove large object descriptor */
    /* remove naming entry */
    oid deloid;
    
    deloid = LOunlinkOID(path);
    if (deloid != InvalidObjectId) { /* remove associated file if any */
	LOunassocOID(deloid);
    }
}

/*
 * ------------------------------------
 * Support routines for this file
 * ------------------------------------
 */
int NewLOfd(lobjCookie,objtype)
     char *lobjCookie;
     int objtype;
{
    int i;

    for (i = 0; i < MAX_LOBJ_FDS; i++) {

	if (cookies[i] == NULL) {
	    cookies[i] = lobjCookie;
	    lotype[i] =  objtype;

	    return i;
	}
    }
    return -1;
}

void DeleteLOfd(fd)
     int fd;
{
    cookies[fd] = NULL;
}
