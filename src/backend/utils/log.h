/*
 * log.h --
 *	POSTGRES error logging definitions.
 *
 * Identification:
 *	$Header$
 */

#ifndef	LogIncluded	/* Include this file only once. */
#define LogIncluded	1

#include "tmp/c.h"

#define NOTICE	0	/* random info - no special action */
#define WARN	-1	/* Warning error - return to known state */
#define FATAL	1	/* Fatal error - abort process */
#define DEBUG	-2	/* debug message */
#define NOIND	-3	/* debug message, don't indent as far */

#define PTIME	0x100	/* prepend time to message */
#define POS	0x200	/* prepend source position to message */
#define USER	0x400	/* send message to user */
#define TERM	0x800	/* send message to terminal */
#define DBLOG	0x1000	/* put message in per db log */
#define SLOG	0x2000	/* put message in system log */
#define ABORT	0x4000	/* abort process after logging */

extern int	Cline;
extern char	*Cfile;
extern int	ElogDebugIndentLevel;

#define TR(v, i, j)	(Cfile = __FILE__,Cline = __LINE__,v[i] & (1 << j))
#define TRM(v, i)	(Cfile = __FILE__,Cline = __LINE__,v[i] != 0)

#define SETR(v, i, j)	v[i] |= (1 << j)
#define SETRNG(v, s, e)	{ int i = s; for (i = s; i <= e; i++) v[i] = -1; }

#define ALLOCV(i)	(long *)calloc(i, sizeof (long))

/*
 * elog --
 *	Old error logging function.
 */
extern
void
elog ARGS((
	const int	level,
	const String	format,
	...
));

#endif	/* !defined(LogIncluded) */
