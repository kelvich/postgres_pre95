
/*
 *  BACKENDSOCK.C
 *
 *  Backend socket communications routines
 */

#include <errno.h>
#include "tmp/c.h"
#include "utils/log.h"

RcsId("$Header$");

#define MAXQUERYSIZE	8192

static int PortFd = -1;		/* socket fd 				 */
static char InBuf[8192];	/* communications buffer, arbitrary size */
static int InIdx;		/* current index			 */
static int InLen;		/* current length

static char OutBuf[8192];	/* output buffer, arbitrary size	 */
static int OutIdx;

extern int DebugMode;		/* from backendsup.c			 */

void LoadSocketBuf ARGS((void));

void
SetPQSocket(fd)
int fd;
{
    PortFd = fd;
    InIdx = 0;
    InLen = 0;
    OutIdx= 0;
}

char
GetPQChar()
{
    if (InIdx == InLen)
	LoadSocketBuf();
    if (InIdx == InLen)
	return(0);
    return(InBuf[InIdx++]);
}

int 
GetPQInt4()
{
    int i;		/* index, count 4 bytes */
    int r = 0;		/* result		*/

    for (i = 0; i < 4; ++i) {
	if (InIdx == InLen)
	    LoadSocketBuf();
	if (InIdx == InLen)
	    elog(FATAL, "GetPQInt4.read error");
	r = (r << 8) | InBuf[InIdx++];
    }
    return(r);
}

void
GetPQStr(buf)
char *buf;
{
    int i;

    for (i = 0; i < MAXQUERYSIZE; ++i) {
	if (InIdx == InLen)
	    LoadSocketBuf();
	if (InIdx == InLen)
	    elog(FATAL, "GetPQStr.read error");
	buf[i] = InBuf[InIdx++];
	if (buf[i] == 0)
	    break;
    }
    if (i == MAXQUERYSIZE)
	elog(FATAL, "GetPQStr.len error");
}

void
LoadSocketBuf()
{
    int n;
    extern int errno;

    for (;;) {
	n = read(PortFd, InBuf, sizeof(InBuf));
	if (n > 0)
	    break;
	if (n < 0) {
	    if (errno != EINTR)
		elog(FATAL, "LoadSocketBuf.read error %d", errno);
	}
    }
    InIdx = 0;
    InLen = n;
}

