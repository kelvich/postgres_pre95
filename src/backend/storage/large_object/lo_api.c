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
#include "utils/large_object.h"
#include "catalog/pg_naming.h"
#include "catalog/pg_lobj.h"
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
    LargeObjectDesc *retval;
    LargeObject *newobj;
    int fd;
    int obj_len, filename_len;

    file_oid = newoid();
    sprintf(filename, "LO%d", file_oid);

    fd = (int) FileNameOpenFile(filename, O_CREAT | O_RDWR, 0666);
    if (fd == -1) return(NULL);

    newobj = (LargeObject *) NewLargeObject(filename, PURE_FILE);

    retval->object = newobj;
    retval->fd = fd;

    /* Log this instance of large object into directory table. */
    {
      oid oidf;
      if ((oidf = FilenameToOID(path)) == InvalidObjectId) { /* new file */
        oidf = LOcreatOID(path,0); /* enter it in system relation */
        /* enter cookie into table */
        if (LOputOIDandLargeObjDesc(oidf,newobj) < 0)
          elog(NOTICE,"LOputOIDandLargeObjDesc failed");
      }
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

    retval->fd = fd;
    retval->object = object;
    return(retval);
}

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
    Assert(obj_desc->object->lo_storage_type == PURE_FILE);

    /* see where we are now */

    pos = FileTell(obj_desc->fd);

    /* do a seek to find number of bytes */

    len = FileSeek(obj_desc->fd, 0L, L_XTND);

    /* seek back to original position */

    FileSeek(obj_desc->fd, pos, L_SET);

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
    Assert(obj_desc->object->lo_storage_type == PURE_FILE);

    nbytes = LARGE_OBJECT_BLOCK * nblocks;
    bytes_read = FileRead(obj_desc->fd, buf, nbytes);
    /* bogus because this assumes that all but one block is read. */
/*
    if (bytes_read == 0) return(0);

    if (bytes_read % LARGE_OBJECT_BLOCK != 0)
        return(bytes_read % LARGE_OBJECT_BLOCK);
    else
        return(LARGE_OBJECT_BLOCK);
*/
    return bytes_read;
}

/*
 * here, bytes_at_end allows an incomplete block to be written.
 * This should ONLY be used to end a file, since we only allow block-oriented
 * seeking.  IF bytes_in
 */

int
LOBlockWrite(obj_desc, buf, n_whole_blocks, bytes_at_end)

LargeObjectDesc *obj_desc;
char *buf;
unsigned long n_whole_blocks, bytes_at_end;

{
    unsigned long totalbytes;
    int ret;

    Assert(PointerIsValid(obj_desc));
    Assert(PointerIsValid(obj_desc->object));
    Assert(obj_desc->object->lo_storage_type == PURE_FILE);

    totalbytes = LARGE_OBJECT_BLOCK * n_whole_blocks + bytes_at_end;

    ret = FileWrite(obj_desc->fd, buf, totalbytes);
    if (ret < 0) {
	perror ("NOTICE: write");
    } else {
	return ret;
    }
}

/*
 * Just like seek, although "offset" is the block offset, and the return
 * value is a block number.
 */

unsigned long
LOBlockSeek(obj_desc, offset, whence)

LargeObjectDesc *obj_desc;
unsigned long offset;
int whence;

{
    unsigned long retval;

    Assert(PointerIsValid(obj_desc));
    Assert(PointerIsValid(obj_desc->object));
    Assert(obj_desc->object->lo_storage_type == PURE_FILE);

    retval = FileSeek(obj_desc->fd, offset * LARGE_OBJECT_BLOCK, whence);

    return(retval / LARGE_OBJECT_BLOCK);
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
    Assert(obj_desc->object->lo_storage_type == PURE_FILE);

    FileClose(obj_desc->fd);
    pfree(obj_desc);
}

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
    Assert(obj_desc->object->lo_storage_type == PURE_FILE);

    retval = obj_desc->object;
    return(retval);
}

/*
 * Returns your standard unix stat(2) information.
 */
int
LOUnixStat(obj_desc, stbuf)

LargeObjectDesc *obj_desc;
struct stat *stbuf;
{
    int ret;

    Assert(PointerIsValid(obj_desc));
    Assert(PointerIsValid(obj_desc->object));
    Assert(obj_desc->object->lo_storage_type == PURE_FILE);
    Assert(stbuf != NULL);

    ret = stat(obj_desc->object->lo_ptr.filename,stbuf);
#if 0
    elog(NOTICE,"LOUnixStat: fd %d, ret %d, size %d blocks %d",obj_desc->fd,
         ret, stbuf->st_size,stbuf->st_blocks);
#endif

    return ret;
}
