/*
 * exc.h --
 *	POSTGRES exception handling definitions.
 *
 * Identification:
 *	$Header$
 */

#ifndef	ExcIncluded
#define ExcIncluded	1	/* Include this only once */

#include "c.h"

#include <setjmp.h>

/*
 * InitExceptionHandling --
 *	Initializes the exception handling system.
 *
 * Note:
 *	This must be called before any exceptions occur.  I.e., call this first!
 *	This routine will not return if an error is detected.
 *
 * Exceptions:
 *	none
 */
extern
void
InitExceptionHandling ARGS((
	bool	firstTime
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

extern	void	ExcRaise(/* Exception*, ExcDetail, ExcData, ExcMessage */);

extern	ExcProc	*ExcGetUnCaught();
extern	ExcProc	*ExcSetUnCaught(/* ExcProc * */);

extern	void	ExcUnCaught(/* Exception*, ExcDetail, ExcData, ExcMessage */);

extern	void	ExcPrint(/* Exception*, ExcDetail, ExcData, ExcMessage */);
extern	char*	ProgramName;

extern	void	ExcAbort(/* Exception*, ExcDetail, ExcData, ExcMessage */);

#endif	/* !defined(ExcIncluded) */
