
; /*
; * 
; * POSTGRES Data Base Management System
; * 
; * Copyright (c) 1988 Regents of the University of California
; * 
; * Permission to use, copy, modify, and distribute this software and its
; * documentation for educational, research, and non-profit purposes and
; * without fee is hereby granted, provided that the above copyright
; * notice appear in all copies and that both that copyright notice and
; * this permission notice appear in supporting documentation, and that
; * the name of the University of California not be used in advertising
; * or publicity pertaining to distribution of the software without
; * specific, written prior permission.  Permission to incorporate this
; * software into commercial products can be obtained from the Campus
; * Software Office, 295 Evans Hall, University of California, Berkeley,
; * Ca., 94720 provided only that the the requestor give the University
; * of California a free licence to any derived software for educational
; * and research purposes.  The University of California makes no
; * representations about the suitability of this software for any
; * purpose.  It is provided "as is" without express or implied warranty.
; * 
; */



/*
 * form.h --
 *	POSTGRES formated tuple (user) attribute values definitions.
 *
 * Identification:
 *	$Header$
 */

#ifndef	FormIncluded	/* Include this file only once. */
#define FormIncluded	1

#include "c.h"

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
