/*
 * exc.c --
 *	POSTGRES exception handling code.
 *
 * Note:
 *	XXX this code needs improvement--check for state violations and
 *	XXX reset after handling an exception.
 */

#include <stdio.h>	/* XXX use own I/O routines */

#include "c.h"

#include "pinit.h"	/* for ExitPostgres */

#include "exc.h"

RcsId("$Header$");

/*
 * Global Variables
 */
static bool ExceptionHandlingEnabled = false;

String			ExcFileName = NULL;
Index			ExcLineNumber = 0;

ExcFrame		*ExcCurFrameP = NULL;

static	ExcProc		*ExcUnCaughtP = NULL;

/*
 * Exported Functions
 */

/*
 * Excection handling should be supported by the language, thus there should
 * be no need to explicitly enable exception processing.
 *
 * This function should probably not be called, ever.  Currently it does
 * almost nothing.  If there is a need for this intialization and checking.
 * then this function should be converted to the new-style Enable code and
 * called by all the other module Enable functions.
 */
void
EnableExceptionHandling(on)
	bool	on;
{
	if (on == ExceptionHandlingEnabled) {
		/* XXX add logging of failed state */
		ExitPostgres(FatalExitStatus);
	}

	if (on) {	/* initialize */
		;
	} else {	/* cleanup */
		ExcFileName = NULL;
		ExcLineNumber = 0;
		ExcCurFrameP = NULL;
		ExcUnCaughtP = NULL;
	}

	ExceptionHandlingEnabled = on;
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
