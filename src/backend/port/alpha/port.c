/*
 *   FILE
 *	port.c
 *
 *   DESCRIPTION
 *	OSF/1-specific routines
 *
 *   INTERFACE ROUTINES
 *	init_address_fixup
 *
 *   NOTES
 *
 *   IDENTIFICATION
 *	$Header$
 */

#include <sys/types.h>
#include <sys/sysinfo.h>
#include <sys/proc.h>

#include "tmp/c.h"

#include "utils/log.h"

RcsId("$Header$");

init_address_fixup()
{
#ifdef NOFIXADE
    int buffer[] = { SSIN_UACPROC, UAC_SIGBUS };
#endif /* NOFIXADE */
#ifdef NOPRINTADE
    int buffer[] = { SSIN_UACPROC, UAC_NOPRINT };
#endif /* NOPRINTADE */

    if (setsysinfo(SSI_NVPAIRS, buffer, 1, (caddr_t) NULL,
		   (unsigned long) NULL) < 0) {
	elog(NOTICE, "setsysinfo failed: %d\n", errno);
    }
}
