
/*
 * 
 * POSTGRES Data Base Management System
 * 
 * Copyright (c) 1988 Regents of the University of California
 * 
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for educational, research, and non-profit purposes and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of the University of California not be used in advertising
 * or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Permission to incorporate this
 * software into commercial products can be obtained from the Campus
 * Software Office, 295 Evans Hall, University of California, Berkeley,
 * Ca., 94720 provided only that the the requestor give the University
 * of California a free licence to any derived software for educational
 * and research purposes.  The University of California makes no
 * representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied warranty.
 * 
 */



/*
 * ppreserve.c --
 *	Routines to preserve palloc'd memory in LISP.
 */

#include "postgres.h"
#include "lispdep.h"

RcsId("$Header$");

/*
 *	ppreserve	- preserve palloc'd memory in LISP
 *
 *	Returns a vectori-byte containing a copy of palloc-formatted
 *	memory.  "Prestore" should be called before trying to use it.
 */
LispValue
ppreserve(pallocObject)
	char	*pallocObject;
{
	LispValue	vectori;
	int		length;

	length = PSIZEALL(pallocObject);
	LISP_GC_OFF;
	vectori = lispVectori(length);
	LISP_GC_PROTECT(vectori);
	LISP_GC_ON;
	bcopy(PSIZEFIND(pallocObject),
	      (char *) LISPVALUE_BYTEVECTOR(vectori),
	      length);
	return(vectori);
}

LispValue
lppreserve(pallocObject)
	LispValue	pallocObject;
{
	return(ppreserve((char *) LISPVALUE_INTEGER(pallocObject)));
}


/*
 *	prestore	- restore saved palloc'd memory
 *
 *	Returns a pointer to the data in a ppreserve'd object.
 *	See "ppreserve" above.
 */
char *
prestore(ppreservedObject)
	char	*ppreservedObject;
{
	return(PSIZESKIP(ppreservedObject));
}

LispValue
lprestore(ppreservedObject)
	LispValue	ppreservedObject;
{
	LispValue	newPointer;

	LISP_GC_OFF;
	newPointer = lispInteger((int)
				 prestore(LISPVALUE_BYTEVECTOR(ppreservedObject)));
	LISP_GC_PROTECT(newPointer);
	LISP_GC_ON;
	return(newPointer);
}
