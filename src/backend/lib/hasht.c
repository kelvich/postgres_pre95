/*
 * hasht.c --
 *	hash table related functions that are not directly supported
 *	by the hashing packages under utils/hash.
 *
 */

#include "tmp/c.h"

RcsId("$Header$");

#include "utils/memutils.h"
#include "utils/log.h"
#include "utils/hsearch.h"

/* -----------------------------------
 *	HashTableWalk
 *
 *	call function on every element in hashtable
 *	one extra argument, arg may be supplied
 * -----------------------------------
 */
void
HashTableWalk(hashtable, function, arg)
HTAB *hashtable;
void (*function)();
int arg;
{
    int *hashent;
    int *data;
    int keysize;

    keysize = hashtable->hctl->keysize;
    (void)hash_seq((HTAB *)NULL);
    while ((hashent = hash_seq(hashtable)) != (int*)TRUE) {
	if (hashent == NULL)
	    elog(FATAL, "error in HashTableWalk.");
	data = (int*)LONGALIGN((char*)hashent + keysize);
	(*function)(data, arg);
    }
}
