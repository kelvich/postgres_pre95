/*
 *  md.c -- magnetic disk storage manager.
 *
 *	This code manages relations that reside on magnetic disk.
 */

#include "tmp/c.h"
#include "tmp/postgres.h"

#include "port/machine.h"

int
mdcreate(path)
    char *path;
{
    int fd;
    int tmp;

    fd = open(path, O_RDWR|O_CREAT|O_EXCL, 0666);

    /*
     *  If the file already exists and is empty, we pretend that the
     *  create succeeded.  This only happens during bootstrap processing,
     *  when pg_time, pg_variable, and pg_log get created before their
     *  .bki file entries are processed.
     */

    if (fd < 0) {
	if (fd = open(path, O_RDWR, 0666) >= 0) {
	    if (read(fd, &tmp, sizeof(tmp)) != 0) {
		close(fd);
		return (-1);
	    }
	}
    }

    return (fd);
}

/*
 *  mdextend() -- Add a block to the specified relation.
 *
 *	This routine returns SM_FAIL or SM_SUCCESS, with errno set as
 *	appropriate.
 */

int
mdextend(reln, buffer)
    Relation reln;
    char *buffer;
{
    long pos;
    File vfd;
    BufferDesc bufhdr;

    vfd = RelationGetFile(reln);

    if ((pos = lseek(vfd, 0L, L_XTND)) < 0)
	return (SM_FAIL);

    if (write(fd, buffer, BLCKSZ) != BLCKSZ)
	return (SM_FAIL);

    return (SM_SUCCESS);
}
