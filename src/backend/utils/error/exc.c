/*
 * exc.c --
 *	POSTGRES exception handling code.
 */

#include <stdio.h>	/* XXX use own I/O routines */

#include "c.h"

#include "pinit.h"	/* for ExitPostgres */

#include "exc.h"

RcsId("$Header$");

/*
 * Global Variables
 */

String			ExcFileName = NULL;
Index			ExcLineNumber = 0;

ExcFrame		*ExcCurFrameP = NULL;

static	ExcProc		*ExcUnCaughtP = NULL;

/*
 * Exported Functions
 */

void
InitExceptionHandling(firstTime)
	bool	firstTime;
{
	static bool	called = false;

	if (firstTime == called) {
		ExitPostgres(FatalExitStatus);
	}

	called = true;

	if (firstTime) {
		/* initialize */
		;
	} else {
		/* do deallocations and cleanup */
		/* reinitialize */
		ExcFileName = NULL;
		ExcLineNumber = 0;
		ExcCurFrameP = NULL;
		ExcUnCaughtP = NULL;
	}
}

void
ExcPrint(excP, detail, data, message)
	Exception	*excP;
	ExcDetail	detail;
	ExcData		data;
	ExcMessage	message;
{
	extern	int	errno;
	extern	int	sys_nerr;
	extern	char	*sys_errlist[];

#ifdef	lint
	data = data;
#endif

	(void) fflush(stdout);	/* In case stderr is buffered */

#if	0
	if (ProgramName != NULL && *ProgramName != '\0')
		(void) fprintf(stderr, "%s: ", ProgramName);
#endif

	if (message != NULL)
		(void) fprintf(stderr, "%s", message);
	else if (excP->message != NULL)
		(void) fprintf(stderr, "%s", excP->message);
	else
#ifdef	lint
		(void) fprintf(stderr, "UNNAMED EXCEPTION 0x%lx", excP);
#else
		(void) fprintf(stderr, "UNNAMED EXCEPTION 0x%lx", (long)excP);
#endif

	(void) fprintf(stderr, " (%ld)", detail);

	if (errno > 0 && errno < sys_nerr &&
	    sys_errlist[errno] != NULL && sys_errlist[errno][0] != '\0')
		(void) fprintf(stderr, " [%s]", sys_errlist[errno]);
	else if (errno != 0)
		(void) fprintf(stderr, " [Error %d]", errno);

	(void) fprintf(stderr, "\n");

	(void) fflush(stderr);
}

/*ARGSUSED*/
void
ExcAbort(excP, detail, data, message)
	Exception	*excP;
	ExcDetail	detail;
	ExcData		data;
	ExcMessage	message;
{
	AbortPostgres();
}

ExcProc *
ExcGetUnCaught()
{
	return (ExcUnCaughtP);
}

ExcProc *
ExcSetUnCaught(newP)
	ExcProc	*newP;
{
	ExcProc	*oldP = ExcUnCaughtP;

	ExcUnCaughtP = newP;

	return (oldP);
}

void
ExcUnCaught(excP, detail, data, message)
	Exception	*excP;
	ExcDetail	detail;
	ExcData		data;
	ExcMessage	message;
{
	ExcPrint(excP, detail, data, message);

	ExcAbort(excP, detail, data, message);
	/*NOTREACHED*/
}

void
ExcRaise(excP, detail, data, message)
	Exception	*excP;
	ExcDetail	detail;
	ExcData		data;
	ExcMessage	message;
{
	register ExcFrame	*efp;

	efp = ExcCurFrameP;
	if (efp == NULL) {
		if (ExcUnCaughtP != NULL)
			(*ExcUnCaughtP)(excP, detail, data, message);

		ExcUnCaught(excP, detail, data, message);
		/*NOTREACHED*/
	} else {
		efp->id		= excP;
		efp->detail	= detail;
		efp->data	= data;
		efp->message	= message;

		ExcCurFrameP = efp->link;

		longjmp(efp->context, 1);
		/*NOTREACHED*/
	}
	/*NOTREACHED*/
}
