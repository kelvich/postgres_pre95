/* ----------------------------------------------------------------
 *   FILE
 *	pqsignal.c
 *
 *   DESCRIPTION
 *	reliable BSD-style signal(2) routine
 *	stolen from RWW who stole it from Stevens...
 *
 *   NOTES
 *	This shouldn't be in libpq, but the monitor and some other
 *	things need it...
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#include "libpq/pqsignal.h"

pqsigfunc
pqsignal(signo, func)
    int signo;
    pqsigfunc func;
{
#if defined(USE_POSIX_SIGNALS)
    struct sigaction act, oact;
    
    act.sa_handler = func;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
    if (signo != SIGALRM) {
	act.sa_flags |= SA_RESTART;
    }
    if (sigaction(signo, &act, &oact) < 0)
	return(SIG_ERR);
    return(oact.sa_handler);
#else /* !USE_POSIX_SIGNALS */
    Assert(0);
#endif /* !USE_POSIX_SIGNALS */
}
