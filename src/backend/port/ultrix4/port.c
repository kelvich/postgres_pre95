/*
 *   FILE
 *	port.c
 *
 *   DESCRIPTION
 *	Ultrix-specific routines
 *
 *   INTERFACE ROUTINES
 *	init_address_fixup
 *
 *   NOTES
 *
 *   IDENTIFICATION
 *	$Header$
 */

#include <sys/syscall.h>
#include <sys/sysmips.h>

#include "tmp/c.h"

RcsId("$Header$");

init_address_fixup()
{
#ifdef NOFIXADE
    syscall(SYS_sysmips, MIPS_FIXADE, 0, NULL, NULL, NULL);
#endif /* NOFIXADE */
}
