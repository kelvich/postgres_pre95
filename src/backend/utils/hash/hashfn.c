/*
 * hashfn.c
 *
 * Identification:
 *	$Header$
 *
 */
#include "utils/hsearch.h"

/*
	Assume that we've already split the bucket to which this
	key hashes, calculate that bucket, and check that in fact
	we did already split it.
*/
int
string_hash(key,keysize)
char *	key;
int	keysize;
{
	int h;
	register unsigned char *k = (unsigned char *) key;

	h = 0;
	/*
	 * Convert string to integer
	 */
	while (*k)
		h = h * PRIME1 ^ (*k++ - ' ');
	h %= PRIME2;

	return (h);
}


tag_hash(key,keysize)
int *	key;
int	keysize;
{
	int h;
	
	/*
	 * Convert tag to integer;
	 */
	for(h = 0;keysize > (sizeof(int)-1);keysize -= sizeof(int),key ++) {
	  h = h * PRIME1 ^ (*key);
	}
	while (keysize) {
	  h = h * PRIME1 ^ (*((char *)key));
	  key = (int *) ((unsigned)key)+1;
	  keysize--;
	}
	h %= PRIME2;
	return (h);
}

/*
 * This is INCREDIBLY ugly, but fast.
 * We break the string up into 8 byte units.  On the first time
 * through the loop we get the "leftover bytes" (strlen % 8).
 * On every other iteration, we perform 8 HASHC's so we handle
 * all 8 bytes.  Essentially, this saves us 7 cmp & branch
 * instructions.  If this routine is heavily used enough, it's
 * worth the ugly coding
 */
int
disk_hash(key)
char *key;
{
        register int n = 0;
	register char *str = key;
	register int len = strlen(key);
	register int loop;

#define HASHC   n = *str++ + 65599 * n

        if (len > 0) {
                loop = (len + 8 - 1) >> 3;

                switch(len & (8 - 1)) {
			case 0: do {		/* All fall throughs */
					HASHC;  
				case 7: HASHC;
				case 6: HASHC;  
				case 5: HASHC;
				case 4: HASHC;  
				case 3: HASHC;
				case 2: HASHC;  
				case 1: HASHC;
                        } while (--loop);
                }

        }
	return(n);
}


