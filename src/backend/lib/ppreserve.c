/*
 * ppreserve.c --
 *	Routines to preserve palloc'd memory in LISP.
 */

#include "tmp/postgres.h"
#include "nodes/pg_lisp.h"

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
	vectori = lispVectori(length);
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

	newPointer = lispInteger((int)
				 prestore(LISPVALUE_BYTEVECTOR(ppreservedObject)));
	return(newPointer);
}
