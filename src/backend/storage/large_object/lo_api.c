/*#define FSDB 1*/
/*
 * large_object.c - routines for manipulating and accessing large objects
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
#include "access/relscan.h"
#include "access/tupdesc.h"
#include "catalog/pg_naming.h"
#include "catalog/pg_lobj.h"
#include "storage/itemptr.h"
#include "utils/rel.h"
#include "utils/large_object.h"
#include "utils/log.h"

LargeObject *NewLargeObject();

/*
 * These routines allow the user to import and export Postgres PURE_FILE
 * large objects.
 */

/*
 * These next routines are the "internal" large object interface for use by
 * user-defined functions.
 */

/*
 * Eventually, LargeObjectCreate should log all filenames in some type of
 * log - this approach will have problems if an object is destroyed.
 */

LargeObjectDesc *
LOCreate(path, open_mode)
char *path;
int open_mode;

{
    char filename[256];
    int file_oid;
    LargeObjectDesc *retval = NULL;
    LargeObject *newobj;
    int fd;
    int obj_len, filename_len;
    oid oidf;


    /* Log this instance of large object into directory table. */
    if ((oidf = FilenameToOID(path)) == InvalidObjectId) {

	/* enter it in system relation */
	if ((oidf = LOcreatOID(path,0)) == InvalidObjectId)
	    elog(WARN, "%s: no such file or directory", path);

	file_oid = newoid();
	sprintf(filename, "LO%d", file_oid);

	fd = (int) FileNameOpenFile(filename, O_CREAT | O_RDWR, 0666);
	if (fd == -1) return(NULL);

	newobj = (LargeObject *) NewLargeObject(filename, PURE_FILE);
	retval = (LargeObjectDesc *) palloc(sizeof(LargeObjectDesc));

	retval->object = newobj;
	retval->ofs.u_fs.fd = fd;

	/* enter cookie into table */
	(void) LOputOIDandLargeObjDesc(oidf, Unix, (struct varlena *) newobj);
    }

    return(retval);
}

LargeObjectDesc *
LOOpen(object, open_mode)
    LargeObject *object;
    int open_mode;
{
    LargeObjectDesc *retval;
    int fd;

    Assert(PointerIsValid(object));
    Assert(object->lo_storage_type == PURE_FILE);

    fd = FileNameOpenFile(object->lo_ptr.filename, open_mode, 0666);

    if (fd == -1) return(NULL);

    retval = (LargeObjectDesc *) palloc(sizeof(LargeObjectDesc));

    retval->ofs.u_fs.fd = fd;
    retval->object = object;
    return(retval);
}

/*
 * These functions deal with ``External'' large objexts,
 * i.e. unix files who file names are interpreted in the name
 * space of the file system running unix.
 */

LargeObjectDesc *
XOCreate(path, open_mode)
char *path;
int open_mode;

{
    LargeObjectDesc *retval = NULL;
    LargeObject *newobj;
    int fd;
    oid oidf;


    /* Log this instance of large object into directory table. */
    if ((oidf = FilenameToOID(path)) == InvalidObjectId) {

	fd = (int) PathNameOpenFile(path, O_CREAT | O_RDWR, 0666);
	if (fd == -1) {
		/* Try once more so that we can incorpate read_only
		   objects.  p_write() to this file descriptor will
		   fail of course! */
		fd = (int) PathNameOpenFile(path, O_RDONLY, 0666);
		if (fd == -1) {
			elog(WARN, "%s: couldn't open", path);
			return(NULL);
		}
	}

	/* enter it in system relation */
	if ((oidf = LOpathOID(path,0)) == InvalidObjectId) {
		FileClose(fd);
		elog(WARN, "%s: couldn't force path name", path);
		return (NULL);
	}

	newobj = (LargeObject *) NewLargeObject(path, EXTERNAL_FILE);
	retval = (LargeObjectDesc *) palloc(sizeof(LargeObjectDesc));

	retval->object = newobj;
	retval->ofs.u_fs.fd = fd;

	/* enter cookie into table */
	(void) LOputOIDandLargeObjDesc(oidf, External,
				(struct varlena *) newobj);
    }

    return(retval);
}

LargeObjectDesc *
XOOpen(object, open_mode)
    LargeObject *object;
    int open_mode;
{
    LargeObjectDesc *retval;
    int fd;

    Assert(PointerIsValid(object));
    Assert(object->lo_storage_type == EXTERNAL_FILE);

    fd = PathNameOpenFile(object->lo_ptr.filename, open_mode, 0666);

    if (fd == -1) return(NULL);

    retval = (LargeObjectDesc *) palloc(sizeof(LargeObjectDesc));

    retval->ofs.u_fs.fd = fd;
    retval->object = object;
    return(retval);
}

#ifdef JAQUITH

/*
 * These functions deal with ``Jaquith'' large objexts,
 * These are stored on the device, and moved to unix when needed.
 * the path name will be mirrored from unix to jaquith
 */

LargeObjectDesc *
JOCreate(path, open_mode)
char *path;
int open_mode;

{
    LargeObjectDesc *retval = NULL;
    LargeObject *newobj;
    int fd, status, i;
    oid oidf;

    if (*path != '/') 
	elog(WARN, "Jaquith file names must start with a '/' ");

    /* Log this instance of large object into directory table. */
    if ((oidf = FilenameToOID(path)) == InvalidObjectId) {
	
	/* enter it in system relation */
	if ((oidf = LOpathOID(path,0)) == InvalidObjectId)
	    elog(WARN, "%s: couldn't force path name", path);

	/* call jaquith, to get the file into the specified path */
	/* jaquith must already have the file in that exact same path */

	/* remove any old file that is there.  FILE WILL BE LOST */
	JO_clean(JO_path(path));

	JO_get(path);
	fd = (int) PathNameOpenFile(JO_path(path), O_CREAT | O_RDWR, 0666);
	
	if (fd == -1) return(NULL);

	newobj = (LargeObject *) NewLargeObject(path, JAQUITH_FILE);
	retval = (LargeObjectDesc *) palloc(sizeof(LargeObjectDesc));

	retval->object = newobj;
	retval->ofs.u_fs.fd = fd;
	retval->ofs.u_fs.dirty = 0;;

	/* enter cookie into table */
	(void) LOputOIDandLargeObjDesc(oidf, Jaquith,
				(struct varlena *) newobj);
    }

    return(retval);
}

LargeObjectDesc *
JOOpen(object, open_mode)
    LargeObject *object;
    int open_mode;
{
    LargeObjectDesc *retval;
    int fd;
    char *path;

    Assert(PointerIsValid(object));
    Assert(object->lo_storage_type == JAQUITH_FILE);

    path = JO_path(object->lo_ptr.filename);

    /* remove any old file that is there.  FILE WILL BE LOST */
    JO_clean(path);

    JO_get(object->lo_ptr.filename);
    fd = PathNameOpenFile(path, open_mode, 0666);

    if (fd == -1) return(NULL);

    retval = (LargeObjectDesc *) palloc(sizeof(LargeObjectDesc));

    retval->ofs.u_fs.fd = fd;
    retval->ofs.u_fs.dirty = 0;  /* keep track if file has been written to */
    retval->object = object;
    return(retval);
}

#endif /* JAQUITH */

/*
 * Returns the number of blocks and the byte offset of the last block of
 * the file.  nblocks * LARGE_OBJECT_BLOCK + byte_offset should be equal to
 * the file size.
 */

void
LOStat(obj_desc, nblocks, byte_offset)
    LargeObjectDesc *obj_desc;
    unsigned int *nblocks, *byte_offset;
{
    unsigned long nbytes;
    unsigned long pos, len;

    Assert(PointerIsValid(obj_desc));
    Assert(PointerIsValid(obj_desc->object));
    Assert(((obj_desc->object->lo_storage_type == PURE_FILE) || (obj_desc->object->lo_storage_type == EXTERNAL_FILE) || (obj_desc->object->lo_storage_type == JAQUITH_FILE)));

    /* see where we are now */

    pos = FileTell(obj_desc->ofs.u_fs.fd);

    /* do a seek to find number of bytes */

    len = FileSeek(obj_desc->ofs.u_fs.fd, 0L, L_XTND);

    /* seek back to original position */

    FileSeek(obj_desc->ofs.u_fs.fd, pos, L_SET);

    *nblocks = (len < 0) ? 0 : len / LARGE_OBJECT_BLOCK;
    *byte_offset = len % LARGE_OBJECT_BLOCK;
}

/*
 * Returns the number of bytes read in the last block - zero on failed read.
 */

int
LOBlockRead(obj_desc, buf, nblocks)
    LargeObjectDesc *obj_desc;
    char *buf;
    unsigned long nblocks;
{
    unsigned long nbytes, bytes_read;

    Assert(PointerIsValid(obj_desc));
    Assert(PointerIsValid(obj_desc->object));
    Assert(((obj_desc->object->lo_storage_type == PURE_FILE) || (obj_desc->object->lo_storage_type == EXTERNAL_FILE) || (obj_desc->object->lo_storage_type == JAQUITH_FILE)));

    nbytes = LARGE_OBJECT_BLOCK * nblocks;
    bytes_read = FileRead(obj_desc->ofs.u_fs.fd, buf, nbytes);

    return bytes_read;
}

/*
 * Closes an existing large object descriptor.
 */

void
LOClose(obj_desc)
    LargeObjectDesc *obj_desc;
{
    Assert(PointerIsValid(obj_desc));
    Assert(PointerIsValid(obj_desc->object));
    Assert(((obj_desc->object->lo_storage_type == PURE_FILE) || (obj_desc->object->lo_storage_type == EXTERNAL_FILE)));

    FileClose(obj_desc->ofs.u_fs.fd);
    pfree(obj_desc);
}

#ifdef JAQUITH

/*  Jaquith close, just ends the stdout stream */

void
JOClose(obj_desc)
    LargeObjectDesc *obj_desc;
{
    Assert(PointerIsValid(obj_desc));
    Assert(PointerIsValid(obj_desc->object));
    Assert((obj_desc->object->lo_storage_type == JAQUITH_FILE));

    FileClose(obj_desc->ofs.u_fs.fd);
    if (obj_desc->ofs.u_fs.dirty) 
	JO_put(obj_desc->object->lo_ptr.filename);
    
    JO_clean(JO_path(obj_desc->object->lo_ptr.filename));
    pfree(obj_desc);
}

#endif /* JAQUITH */

/*
 * Destroys an existing large object, and frees its associated pointers.
 * Currently deletes the large object file.
 */

void
LODestroy(object)
    LargeObject *object;
{
    Assert(PointerIsValid(object));
    Assert(object->lo_storage_type == PURE_FILE);

    FileNameUnlink(object->lo_ptr.filename);
    pfree(object);
}

void
XODestroy(object)
    LargeObject *object;
{
    Assert(PointerIsValid(object));
    Assert((object->lo_storage_type == EXTERNAL_FILE) || (object->lo_storage_type == JAQUITH_FILE));

    pfree(object);
}

/*
 * Destroys an existing large object, but doesn't touch memory
 * Currently deletes the large object file.
 */

void
LODestroyRef(object)
    LargeObject *object;
{
    Assert(PointerIsValid(object));
    Assert(object->lo_storage_type == PURE_FILE);

    FileNameUnlink(object->lo_ptr.filename);

}

void
XODestroyRef(object)
    LargeObject *object;
{
    Assert(PointerIsValid(object));
    Assert((object->lo_storage_type == EXTERNAL_FILE) || (object->lo_storage_type == JAQUITH_FILE));
}

/*
 * To be called at the end of the use of a large object, just before the
 * large object is closed.
 */

LargeObject *
LODescToObject(obj_desc)
    LargeObjectDesc *obj_desc;
{
    LargeObject *retval;

    Assert(PointerIsValid(obj_desc));
    Assert(PointerIsValid(obj_desc->object));

    retval = obj_desc->object;
    return(retval);
}

/*
 * Returns your standard unix stat(2) information.
 */
int
LOUnixStat(obj_desc, stbuf)
    LargeObjectDesc *obj_desc;
    struct pgstat *stbuf;
{
    int ret;

    Assert(PointerIsValid(obj_desc));
    Assert(PointerIsValid(obj_desc->object));
    Assert(((obj_desc->object->lo_storage_type == PURE_FILE) || (obj_desc->object->lo_storage_type == EXTERNAL_FILE) || (obj_desc->object->lo_storage_type == JAQUITH_FILE)));
    Assert(stbuf != NULL);

    ret = FileStat(obj_desc->ofs.u_fs.fd,stbuf);

    return ret;
}

/*
 * Handles your standard unix seek(2).
 */
int
LOSeek(obj_desc,offset,whence)
	LargeObjectDesc *obj_desc;
	int offset, whence;
{
    int ret;

    Assert(PointerIsValid(obj_desc));
    Assert(PointerIsValid(obj_desc->object));
    Assert((obj_desc->object->lo_storage_type == PURE_FILE) || (obj_desc->object->lo_storage_type == EXTERNAL_FILE) || (obj_desc->object->lo_storage_type == JAQUITH_FILE));

    ret = FileSeek(obj_desc->ofs.u_fs.fd,offset,whence);

    return ret;
}

/*
 * Handles your standard unix tell(2).
 */
int
LOTell(obj_desc)
	LargeObjectDesc *obj_desc;
{
    int ret;

    Assert(PointerIsValid(obj_desc));
    Assert(PointerIsValid(obj_desc->object));
    Assert(((obj_desc->object->lo_storage_type == PURE_FILE) || (obj_desc->object->lo_storage_type == EXTERNAL_FILE) || (obj_desc->object->lo_storage_type == JAQUITH_FILE)));

    ret = FileTell(obj_desc->ofs.u_fs.fd);

    return ret;
}

/* byte oriented read.
 * maybe do partial block reads
 */

int LORead(obj_desc,buf,n)
     LargeObjectDesc *obj_desc;
     char *buf;
     int n;
{
    int ret;
    Assert(PointerIsValid(obj_desc));
    Assert(PointerIsValid(obj_desc->object));
    Assert(((obj_desc->object->lo_storage_type == PURE_FILE) || (obj_desc->object->lo_storage_type == EXTERNAL_FILE) || (obj_desc->object->lo_storage_type == JAQUITH_FILE)));
    Assert(buf != NULL);

    ret = FileRead(obj_desc->ofs.u_fs.fd,buf,n);

    return ret;
}

/* byte oriented write.
 * maybe do partial block writes
 */

int LOWrite (obj_desc,buf,n)
     LargeObjectDesc *obj_desc;
     char *buf;
     int n;
{
    int ret;
    Assert(PointerIsValid(obj_desc));
    Assert(PointerIsValid(obj_desc->object));
    Assert(((obj_desc->object->lo_storage_type == PURE_FILE) || (obj_desc->object->lo_storage_type == EXTERNAL_FILE) || (obj_desc->object->lo_storage_type == JAQUITH_FILE)));
    Assert(buf != NULL);

    ret = FileWrite(obj_desc->ofs.u_fs.fd,buf,n);
    obj_desc->ofs.u_fs.dirty = 1;  /* only Jaquith will look at this */
        
    return ret;
}
