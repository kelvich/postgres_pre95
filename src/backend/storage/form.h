/*
 * form.h --
 *	POSTGRES formated tuple (user) attribute values definitions.
 *
 * Identification:
 *	$Header$
 */

#ifndef	FormIncluded	/* Include this file only once. */
#define FormIncluded	1

#include "tmp/c.h"

/*
 * FormData --
 *	Formated (aligned) data.
 *
 * Note:
 *	Currently assumes that long alignment is the most strict alignment
 *	needed.
 */
typedef struct FormData {
	long	filler;
} FormData;

typedef Pointer	Form;

/*
 * FormIsValid --
 *	True iff the formated tuple attribute values is valid.
 */
extern
bool
FormIsValid ARGS((
	Form	form
));

#endif	/* !defined(FormIncluded) */
