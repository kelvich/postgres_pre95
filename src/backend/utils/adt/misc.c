/* RcsId("$Header$"); */

#include <sys/file.h>
#include "tmp/c.h"

#ifndef MAO_VLDB

int32
userfntest(i)
    int i;
{
    return (i);
}

#else /* MAO_VLDB */

#include "tmp/libpq-fs.h"
#include "access/relscan.h"
#include "access/tupdesc.h"
#include "catalog/pg_naming.h"
#include "catalog/pg_lobj.h"
#include "storage/itemptr.h"
#include "utils/rel.h"
#include "utils/large_object.h"
#include "utils/log.h"

extern int u_write();
extern int u_read();

int32
userfntest(i)
	int i;
{
	switch (i) {
	    case 0:
		return (u_write());

	    case 1:
		return (u_read());

	    default:
		elog(WARN, "userfntest groks zero and one");
	}
}

int
u_write()
{
	File vfd;
	int fd;
	int tot, nbytes, nwritten;
	struct varlena *buf;

	if ((vfd = PathNameOpenFile("/vmunix", O_RDONLY, 0666)) < 0)
	    elog(WARN, "cannot open /vmunix");

	if ((fd = LOcreat("/maofile", INV_READ|INV_WRITE, Inversion)) < 0)
	    elog(WARN, "cannot create /maofile on inversion");

	FileSeek(vfd, 0L, L_SET);

	buf = (struct varlena *) palloc(8092 + sizeof(int32));

	tot = 0;
	while ((nbytes = FileRead(vfd, buf->vl_dat, 8092)) > 0) {
	    buf->vl_len = nbytes + sizeof(int32);
	    nwritten = LOwrite(fd, buf);
	    tot += nwritten;
	}

	FileClose(vfd);
	LOclose(fd);

	return (tot);
}

int
u_read()
{
	File vfd;
	int fd;
	int tot, nread;
	struct varlena *buf;

	if ((vfd = FileNameOpenFile("eep", O_RDWR|O_CREAT, 0666)) < 0)
	    elog(WARN, "cannot open eep");

	if ((fd = LOopen("/maofile", INV_READ|INV_WRITE, Inversion)) < 0)
	    elog(WARN, "cannot open /maofile on inversion");

	FileSeek(vfd, 0L, L_SET);
	LOlseek(fd, 0L, L_SET);

	buf = (struct varlena *) palloc(8092 + sizeof(int32));

	tot = 0;
	while ((buf = (struct varlena *) LOread(fd, 8092)) > 0) {
	    nread = buf->vl_len - sizeof(int32);
	    FileWrite(vfd, buf->vl_dat, nread);
	    pfree(buf);
	    tot += nread;
	}

	FileClose(vfd);
	LOclose(fd);

	return (tot);
}
#endif /* ndef MAO_VLDB */
