/*
 *  smgr.c -- public interface routines to storage manager switch.
 *
 *	All file system operations in POSTGRES dispatch through these
 *	routines.
 */

#include "tmp/c.h"
#include "tmp/postgres.h"

#include "port/machine.h"
#include "storage/smgr.h"

typedef struct f_smgr {
    int		(*smgr_create))(),
    int		(*smgr_unlink))(),
    int		(*smgr_extend))(),
    int		(*smgr_open))(),
    int		(*smgr_close))(),
    int		(*smgr_read))(),
    int		(*smgr_write))(),
    int		(*smgr_flush))(),
    int		(*smgr_nblocks))(),
    int		(*smgr_commit))(),
    int		(*smgr_abort))()
} f_smgr;

extern int	mdcreate(), mdunlink();
extern int	mdextend(), mdopen(), mdclose(), mdread();
extern int	mdwrite(), mdflush(), mdnblocks(), mdcommit(), mdabort();

extern int	sjcreate(), sjunlink();
extern int	sjextend(), sjopen(), sjclose(), sjread();
extern int	sjwrite(), sjflush(), sjnblocks(), sjcommit(), sjabort();

static f_smgr smgrsw[] = {

    /* magnetic disk */
    { mdcreate, mdunlink, mdextend, mdopen, mdclose, mdread, mdwrite,
      mdflush, mdnblocks, mdcommit, mdabort },

    /* sony jukebox */
    { sjcreate, sjunlink, sjextend, sjopen, sjclose, sjread, sjwrite,
      sjflush, sjnblocks, sjcommit, sjabort }
};

static int NSmgr = lengthof(smgrsw);

/*
 *  smgrcreate() -- Create a new relation by name.
 *
 *	This routine creates the named object on the appropriate storage
 *	device, and returns a file descriptor for it.
 */

int
smgrcreate(which, relname)
    int16 which;
    char *relname;
{
    int fd;

    Assert(which >= 0 && which < NSmgr);

    if ((fd = (*(smgrsw[which].smgr_create))(relname)) < 0)
	elog(WARN, "cannot open %s: %m", relname);

    return (fd);
}

/*
 *  smgrunlink() -- Unlink a relation by name.
 *
 *	The appropriate object is removed from the store.
 */

int
smgrunlink(which, relname)
    int16 which;
    char *relname;
{
    int status;

    Assert(which >= 0 && which < NSmgr);

    if ((status = (*(smgrsw[which].smgr_unlink))(relname)) == SM_FAIL)
	elog(WARN, "cannot unlink %s: %m", relname);

    return (status);
}
int
smgrextend(which, reln, bufhdr)
    int16 which;
    Relation reln;
    BufferDesc bufhdr;
{
    int status;

    Assert(which >= 0 && which < NSmgr);

    /* new blocks are zero-filled */
    bzero((char *) MAKE_PTR(bufhdr->data), BLCKSZ);

    status = (*(smgrsw[which].smgr_extend))(reln, MAKE_PTR(bufhdr->data));

    if (status == SM_FAIL)
	elog(WARN, "%16s: cannot extend: %m", reln->rd_rel->relname);

    return (status);
}

int
smgropen(which, path)
    int16 which;
    char *path;
{
    Assert(which >= 0 && which < NSmgr);

    return ((*(smgrsw[which].smgr_open))(path));
}

int
smgrclose(which, file)
    int16 which;
    File file;
{
    Assert(which >= 0 && which < NSmgr);

    return ((*(smgrsw[which].smgr_close))(file));
}

int
smgrread(which, reln, blocknum)
    int16 which;
    Relation reln;
    BlockNumber blocknum;
{
    Assert(which >= 0 && which < NSmgr);

    return ((*(smgrsw[which].smgr_read))(reln, blocknum));
}

int
smgrwrite(which, bufhdr)
    int16 which;
    BufferDesc bufhdr;
{
    Assert(which >= 0 && which < NSmgr);

    return ((*(smgrsw[which].smgr_write))(bufhdr));
}

int
smgrflush(which, bufhdr)
    int16 which;
    BufferDesc bufhdr;
{
    Assert(which >= 0 && which < NSmgr);

    return ((*(smgrsw[which].smgr_flush))(bufhdr));
}

int
smgrnblocks(which, reln)
    int16 which;
    Relation reln;
{
    Assert(which >= 0 && which < NSmgr);

    return ((*(smgrsw[which].smgr_nblocks))(reln));
}

int
smgrcommit(which)
    int16 which;
{
    Assert(which >= 0 && which < NSmgr);

    return ((*(smgrsw[which].smgr_commit))());
}

int
smgrabort(which)
    int16 which;
{
    Assert(which >= 0 && which < NSmgr);

    return ((*(smgrsw[which].smgr_abort))());
}
