/* ----------------------------------------------------------------
 *   FILE
 *	pqsignal.h
 *
 *   DESCRIPTION
 *	prototypes for the reliable BSD-style signal(2) routine.
 *
 *   NOTES
 *	This shouldn't be in libpq, but the monitor and some other
 *	things need it...
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#ifndef pqsignalIncluded		/* include this file only once */
#define pqsignalIncluded	1

#include <signal.h>

#include "tmp/c.h"

typedef void (*pqsigfunc) ARGS((int));

extern pqsigfunc pqsignal ARGS((int signo, pqsigfunc func));

#if defined(USE_POSIX_SIGNALS)
#define	signal(signo, handler)	pqsignal(signo, (pqsigfunc) handler)
#endif /* USE_POSIX_SIGNALS */

#endif /* pqsignalIncluded */
