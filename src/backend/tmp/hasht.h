/*
 * hasht.h--
 *	hash table related functions that are not directly supported
 *	under utils/hash.
 *
 * Identification:
 *	$Header$
 *
 */

#ifndef	HashTIncluded		/* Include this file only once */
#define HashTIncluded	1

#include "utils/hsearch.h"

void HashTableWalk ARGS((HTAB *hashtable, void (*func)(), int arg));

#endif	/* !defined(HashTIncluded) */
