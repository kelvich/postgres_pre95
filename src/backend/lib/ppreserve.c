/*
 * ppreserve.c --
 *	Routines to preserve palloc'd memory in LISP.
 */

#include "postgres.h"
#include "pg_lisp.h"

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
