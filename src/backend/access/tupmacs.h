/*
 * Tuple macros used by both index tuples and heap tuples.
 * 
 * "$Header$"
 */

#ifndef TUPMACS_H
#define TUPMACS_H

#define att_isnull(ATT, BITS) (!((BITS)[(ATT) >> 3] & ((ATT) & 0x07)))

#define fetchatt(A, T) \
((*(A))->attlen < 0 ? (char *) (LONGALIGN((T) + sizeof(long))) : \
 ((*(A))->attbyval ? \
  ((*(A))->attlen > sizeof(short) ? (char *) *(long *) (T) : \
   ((*(A))->attlen < sizeof(short) ? (char *) *(T) : \
	(char *) * (short *) (T))) : (char *) (T)))
	
#endif

