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

typedef void (*hasht_func)();

void HashTableWalk ARGS((HTAB *hashtable, hasht_func func, int arg));

#endif	/* !defined(HashTIncluded) */
