/*
 * exc.h --
 *	POSTGRES exception handling definitions.
 */

#ifndef	ExcIncluded		/* Include this file only once */
#define ExcIncluded	1

/*
 * Identification:
 */
#define EXC_H	"$Header$"

#include <setjmp.h>

#include "tmp/c.h"

/*
 * EnableExceptionHandling --
 *	Enables/disables the exception handling system.
 *
 * Note:
 *	This must be called before any exceptions occur.  I.e., call this first!
 *	This routine will not return if an error is detected.
 *	This does not follow the usual Enable... protocol.
 *	This should be merged more closely with the error logging and tracing
 *	packages.
 *
 * Exceptions:
 *	none
 */
extern
void
EnableExceptionHandling ARGS((
	bool	on
));

/* START HERE */

/*
 * ExcMessage and Exception are now defined in c.h
 */
#if	0
typedef char*		ExcMessage;

typedef struct Exception {
	ExcMessage	message;
} Exception;
#endif	/* 0 */

typedef jmp_buf		ExcContext;
typedef Exception*	ExcId;
typedef long		ExcDetail;
typedef char*		ExcData;

typedef struct ExcFrame {
	struct ExcFrame	*link;
	ExcContext	context;
	ExcId		id;
	ExcDetail	detail;
	ExcData		data;
	ExcMessage	message;
} ExcFrame;

extern	ExcFrame*	ExcCurFrameP;

#define	ExcBegin()							\
	{								\
		ExcFrame	exception;				\
									\
		exception.link = ExcCurFrameP; 				\
		if (setjmp(exception.context) == 0) {			\
			ExcCurFrameP = &exception;			\
			{
#define	ExcExcept()							\
			}						\
			ExcCurFrameP = exception.link;			\
		} else {						\
			{
#define	ExcEnd()							\
			}						\
		}							\
	}

#define raise(exception)	raise2((exception), 0)
#define raise2(x, detail)	raise3((x), (detail), 0)
#define raise3(x, t, data)	raise4((x), (t), (data), 0)

#define raise4(x, t, d, message) \
	ExcRaise(&(x), (ExcDetail)(t), (ExcData)(d), (ExcMessage)(message))

#define	reraise() \
	raise4(*exception.id,exception.detail,exception.data,exception.message)

typedef	void	ExcProc(/* Exception*, ExcDetail, ExcData, ExcMessage */);

void ExcRaise ARGS((
	Exception *excP,
	ExcDetail detail,
	ExcData data,
	ExcMessage message
));

ExcProc *ExcGetUnCaught ARGS((void ));
ExcProc *ExcSetUnCaught ARGS((ExcProc *newP ));

void ExcUnCaught ARGS((
	Exception *excP,
	ExcDetail detail,
	ExcData data,
	ExcMessage message
));

void ExcPrint ARGS((
	Exception *excP,
	ExcDetail detail,
	ExcData data,
	ExcMessage message
));
extern	char*	ProgramName;

/*
 * ExcAbort --
 *	Handler for uncaught exception.
 *
 * Note:
 *	Define this yourself if you don't want the default action (dump core).
 */
extern
void
ExcAbort ARGS((
	Exception	*excP,
	ExcDetail	detail,
	ExcData		data,
	ExcMessage	message
));

#endif	/* !defined(ExcIncluded) */
