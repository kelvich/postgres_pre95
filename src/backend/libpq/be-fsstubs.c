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
 * int LOcreat(char *path, int mode);
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
 * int LOcreat(char *path, int mode);
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
#include "tmp/libpq.h"
#include "tmp/libpq-fs.h"
#include "utils/large_object.h"
#include "utils/mcxt.h"
#include "catalog/pg_lobj.h"
#include "catalog/pg_naming.h"

#include "utils/log.h"

static GlobalMemory fscxt = NULL;

int LOopen(fname,mode)
     char *fname;
     int mode;
{
    oid loOID;
    LargeObject *lobjCookie;
    LargeObjectDesc *lobjDesc;
    int fd;
    MemoryContext currentContext;

    loOID = FilenameToOID(fname);

    if (loOID == InvalidObjectId) { /* lookup failed */
	if ((mode  & O_CREAT) != O_CREAT) {
	    return -1;
	} else {
	    return LOcreat(fname,mode);
	}
    }
    
    lobjCookie = LOassocOIDandLargeObjDesc(loOID);
    
    if (lobjCookie == NULL) { /* lookup failed, perhaps a dir? */
	return -1;
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
	/* Kemnitz large objects */
	lobjDesc = LOOpen(lobjCookie,mode);

	if (lobjDesc == NULL) {
	    MemoryContextSwitchTo(currentContext);
	    return -1;
	}
	fd = NewLOfd(lobjDesc);
	/* switch context back to orig. */
	MemoryContextSwitchTo(currentContext);
    }
    return fd;
}

#define MAX_LOBJ_FDS 256
static LargeObjectDesc *cookies[MAX_LOBJ_FDS];
static int current_objaddr[MAX_LOBJ_FDS]; /* current address in LO's terms */
static int current_objoffset[MAX_LOBJ_FDS]; /* offset in LO's terms */

int LOclose(fd)
     int fd;
{
    void DeleteLOfd();

    if (fd >= MAX_LOBJ_FDS) {
	elog(WARN,"LOclose(%d) out of range",fd);
	return -2;
    }
    if (cookies[fd] == NULL) {
	elog(WARN,"LOclose(%d)",fd);
	return -3;
    }
    /* Kemnitz large objects */
    {
	MemoryContext currentContext;
	Assert(fscxt != NULL);
	currentContext = MemoryContextSwitchTo((MemoryContext)fscxt);
	
	LOClose(cookies[fd]);
	MemoryContextSwitchTo(currentContext);
    }
    DeleteLOfd(fd);
    return 0;
}

/* XXX Messy LOread. Fix later.
 *  The mere complexity of the read/write functions dictate that they
 *  shouldn't work, or be entirely understandable.
 */
struct varlena * LOread(fd,len)
     int fd;
     int len;
{
    char buf[LARGE_OBJECT_BLOCK];
    struct varlena *retval;
    int bytestoread;
    int totalread = 0;
#define min(x,y)  ( (x) < (y) ? (x) : (y) )

    retval = (struct varlena *)palloc(sizeof(int32) + len);
    bytestoread = len;
#if 0
    elog(NOTICE,"LORead: bytestoread %d current_offset %d current_block %d",
	 bytestoread,current_objoffset[fd],
	 current_objaddr[fd]);
#endif
    /* read leading partial block */
    if (current_objoffset[fd] > 0) {
	int bytesread = LOBlockRead(cookies[fd],buf,1);
	int remainder;
	int actuallyused;
	if (bytesread > 0) {
#if 0
	    elog(NOTICE,"LORead: partial bytesread %d bytestoread %d current_offset %d current_block %d",
		 bytesread,bytestoread,current_objoffset[fd],
		 current_objaddr[fd]);
#endif
	    remainder = bytesread - current_objoffset[fd];
	    actuallyused = min(remainder,bytestoread);
	    bcopy(buf+current_objoffset[fd],VARDATA(retval),
		  actuallyused);
	    current_objoffset[fd] += actuallyused;
	    bytestoread -= actuallyused;
	    totalread += actuallyused;
	    if (current_objoffset[fd] == LARGE_OBJECT_BLOCK) {
		current_objaddr[fd] ++ ;
		current_objoffset[fd] = 0;
	    }
	} else { /* read failed */
#if 0
	    elog(NOTICE,"LORead: partial bytesread %d bytestoread %d current_offset %d current_block %d",
		 bytesread,bytestoread,current_objoffset[fd],
		 current_objaddr[fd]);
#endif

	}
    }
    /* read full blocks + trailing partial block */
    if (bytestoread > 0) {
	int blockstoread = bytestoread / LARGE_OBJECT_BLOCK;
	int remaindertoread = bytestoread % LARGE_OBJECT_BLOCK;
	int bytesread =
	  LOBlockRead(cookies[fd],VARDATA(retval)+totalread,blockstoread);
	bytestoread -= bytesread;
	totalread += bytesread;
	current_objoffset[fd] = bytesread % LARGE_OBJECT_BLOCK;
	current_objaddr[fd] += bytesread/LARGE_OBJECT_BLOCK;

	/* need to read trailing partial?
	   bytestoread < LARGE_OBJECT_BLOCK at this point*/
	if ((bytestoread > 0) && (bytestoread < LARGE_OBJECT_BLOCK)) {
	    int bytesread = LOBlockRead(cookies[fd],buf,1);
	    int actuallyused;
	    actuallyused = min(bytesread,bytestoread);
	    bcopy(buf,VARDATA(retval)+totalread,actuallyused);
	    totalread += actuallyused;
	    bytestoread -= actuallyused;
	    current_objoffset[fd] = actuallyused;
	    LOBlockSeek(cookies[fd],current_objaddr[fd],SEEK_SET);
	}
    }

    VARSIZE(retval) = totalread;
    return retval;
}

/* XXX see LOread comment */
int LOwrite(fd,wbuf)
     int fd;
     struct varlena *wbuf;
{
    int totalwritten = 0;
    int bytestowrite;
    char buf[LARGE_OBJECT_BLOCK];

    bytestowrite = VARSIZE(wbuf);

#if 0
    elog(NOTICE,"LOwrite(%d,%x) bytes %d",fd,wbuf,VARSIZE(wbuf));
#endif

    /*
     * Logically, we can write on byte boundaries, but we actually write
     * complete blocks at a time.
     *  Portion written consists of a remainder partial block, full block and 
     * initial last block.  The current block and current offset pointer are 
     * updated.
     *   <------>
     * [  ][  ][  ][  ]
     *  Cb,Co NCb,NCo
     */
    /* write leading partial block */
    if (current_objoffset[fd] > 0) {
	int remainder = LARGE_OBJECT_BLOCK - current_objoffset[fd];
	int actuallywritten;
	LOBlockRead(cookies[fd],buf,1);
	/* back to previous block */
	LOBlockSeek(cookies[fd],current_objaddr[fd],SEEK_SET);
	bcopy(VARDATA(wbuf),buf+current_objoffset[fd],
	      min(remainder,bytestowrite));
	actuallywritten = min(remainder,bytestowrite);
	LOBlockWrite(cookies[fd],buf,0,
		     current_objoffset[fd]+actuallywritten);
	
	current_objoffset[fd] += actuallywritten;
	if (current_objoffset[fd] == LARGE_OBJECT_BLOCK) {
	    current_objoffset[fd] = 0;
	    current_objaddr[fd] ++;
	} else { /* seek back to previous block */
	    LOBlockSeek(cookies[fd],current_objaddr[fd],SEEK_SET);
	}

	bytestowrite -= actuallywritten;
	totalwritten += actuallywritten;
    }

    /* write full blocks and trailing partial */
    if (bytestowrite > 0) {
	int blockstowrite =  bytestowrite / LARGE_OBJECT_BLOCK;
	int remaindertowrite = bytestowrite % LARGE_OBJECT_BLOCK;
	int byteswritten;
	byteswritten = LOBlockWrite(cookies[fd],VARDATA(wbuf)+totalwritten,
				    blockstowrite,
				    remaindertowrite);
	totalwritten += byteswritten;
	/* incorrect for partial writes */
	current_objaddr[fd] += byteswritten/LARGE_OBJECT_BLOCK;
	current_objoffset[fd] = byteswritten % LARGE_OBJECT_BLOCK;
	if (current_objoffset[fd] != 0) /* back up over incomplete blocks */
	  LOBlockSeek(cookies[fd],current_objaddr[fd],SEEK_SET);
    }

    return totalwritten;
}

int LOlseek(fd,offset,whence)
     int fd,offset,whence;
{
    if (fd >= MAX_LOBJ_FDS) {
	elog(WARN,"LOSeek(%d) out of range",fd);
	return -2;
    }
    if (cookies[fd] == NULL) {
	elog(WARN,"LOSeek(%d) cookie is NULL",fd);
	return -3;
    }
    /* Kemnitz large objects */
    current_objaddr[fd] =
      LOBlockSeek(cookies[fd],offset/LARGE_OBJECT_BLOCK,whence);
    current_objoffset[fd] = offset % LARGE_OBJECT_BLOCK;
#if 0
    elog(NOTICE,"LOSeek(%d,%d,%d) curblock %d, curoffset %d",fd,offset,whence,
	 current_objaddr[fd],current_objoffset[fd]);
#endif
    return current_objaddr[fd]*LARGE_OBJECT_BLOCK + current_objoffset[fd];
}

int LOcreat(path,mode)
     char *path;
     int mode;
{
    LargeObjectDesc *lobjDesc;
    int fd;

    elog(NOTICE,"LOCreat(%s,%d)",path,mode);
    {
	MemoryContext currentContext;
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
	/* Kemnitz large objects */
	lobjDesc = LOCreate(path,mode);

	if (lobjDesc == NULL) {
	    MemoryContextSwitchTo(currentContext);
	    return -1;
	}
	fd = NewLOfd(lobjDesc);
	/* switch context back to orig. */
	MemoryContextSwitchTo(currentContext);
    }
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
    return current_objaddr[fd]*LARGE_OBJECT_BLOCK + current_objoffset[fd];
}

int LOftruncate()
{
}

struct varlena *LOstat(path)
     char *path;
{
    struct varlena *ret;
    struct stat *st;
    unsigned int nblocks, byte_offset;
    int len;
    len = sizeof(struct stat);
    ret = (struct varlena *) palloc(len+sizeof(int32));
    VARSIZE(ret) = len;
    st = (struct stat *)VARDATA(ret);
    bzero(st,len);	/* default values of 0 */
#if 0
    if (!LOisdir(path)) {
	/* kemnitz large objects */
	int fd = LOopen(path,O_RDONLY);
	LOUnixStat(cookies[fd],st);
/*	elog(NOTICE,"LOstat: fd %d file %s size %d blocks %d",fd,
	     path,st->st_size,st->st_blocks);*/
	LOStat(cookies[fd],&nblocks,&byte_offset);
	LOclose(fd);
/*	st->st_size = nblocks*LARGE_OBJECT_BLOCK + byte_offset;
	st->st_mode = S_IFREG;
	st->st_mode |= S_IRUSR|S_IWUSR|S_IXUSR|
	  S_IRGRP|S_IWGRP|S_IXGRP|
	    S_IROTH|S_IWOTH|S_IXOTH;*/
	st->st_blksize = LARGE_OBJECT_BLOCK;
	st->st_blocks = nblocks+ (byte_offset?1:0);
    } else if (FilenameToOID(path) != InvalidObjectId) {
	st->st_mode = S_IFDIR;
	/* our directoiries don't exist in the filesystem, so give them
	   artificial permissions */
	st->st_mode |= S_IRUSR|S_IWUSR|S_IXUSR|
	  S_IRGRP|S_IWGRP|S_IXGRP|
	    S_IROTH|S_IWOTH|S_IXOTH;
    } else {
	VARSIZE(ret) = 5;
    }
#endif    
    return ret;
}

/*
  LOrename can be found in naming.c
*/

int LOmkdir(path,mode)
     char *path;
     int mode;
{
    oid oidf;
    /* enter new pathname */
    oidf = LOcreatOID(path,mode);
    if (oidf == InvalidObjectId) {
	elog(NOTICE,"LOcreatOID failed");
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
    else
      LOunlink(path);
}

int LOunlink(path)
     char *path;
{
    /* remove large object descriptor */
    /* remove naming entry */
    LOunlinkOID(path);
}

/*
 * ------------------------------------
 * Support routines for this file
 * ------------------------------------
 */
int NewLOfd(lobjCookie)
     LargeObjectDesc *lobjCookie;
{
    int i;
    for (i = 0; i < MAX_LOBJ_FDS; i++) {
	if (cookies[i] == NULL) {
	    cookies[i] = lobjCookie;
	    current_objaddr[i] = 0;
	    current_objoffset[i] = 0;
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
