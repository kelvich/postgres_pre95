/*
 * excabort.c --
 *	Default exception abort code.
 */

#include "c.h"

RcsId("$Header$");

#include "exc.h"

/*ARGSUSED*/
void
ExcAbort(excP, detail, data, message)
	Exception	*excP;
	ExcDetail	detail;
	ExcData		data;
	ExcMessage	message;
{
#ifdef	__SABER__
	saber_stop();
#else
	/* dump core */
	abort();
#endif
}
