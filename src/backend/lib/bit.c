
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
 * bit.c --
 *	Standard bit array code.
 *
 * Identification:
 *	$Header$
 */

#include "c.h"

#include "bit.h"

int
NumberOfBitsPerByte()
{
	return(BitsPerByte);
}

void
BitArraySetBit(bitArray, bitIndex)
	BitArray	bitArray;
	BitIndex	bitIndex;
{	
	bitArray[bitIndex / BitsPerByte] |= (1 << 
			(BitsPerByte - (bitIndex % BitsPerByte) - 1));
	return;
}

void
BitArrayClearBit(bitArray, bitIndex)
	BitArray	bitArray;
	BitIndex	bitIndex;
{
	bitArray[bitIndex / BitsPerByte] &= ~(1 << 
			(BitsPerByte - (bitIndex % BitsPerByte) - 1));
	return;
}

bool
BitArrayBitIsSet(bitArray, bitIndex)
	BitArray	bitArray;
	BitIndex	bitIndex;
{	
	return( (bool) (((bitArray[bitIndex / BitsPerByte] &
			  (1 << (BitsPerByte - (bitIndex % BitsPerByte)
					    - 1)
			  )
			 ) != 0 ) ? 1 : 0) );
}

void
BitArrayZero(bitArray, bitIndex, numberOfBits)
	BitArray	bitArray;
	BitIndex	bitIndex;	/* start bit */
	BitIndex	numberOfBits;	/* number of Bits to set to zero */
{
	long	i;
	long	n;
	BitIndex	endIndex;
	unsigned long	startByte, stopByte;	/* XXX */

	n = numberOfBits;
	startByte = bitIndex / BitsPerByte;
	endIndex = bitIndex + numberOfBits -1;
	stopByte = (bitIndex + numberOfBits) / BitsPerByte;

	bitArray[startByte]  &= 
	       ((~0 << (BitsPerByte - (bitIndex % BitsPerByte))) |
		(~0 >> (Min(BitsPerByte - 1, bitIndex + numberOfBits - 1) + 1))
	       );
	for (i = startByte + 1; i < stopByte; i++)
		bitArray[i]  &= 0;
	if (stopByte > startByte)
		bitArray[stopByte]  &=  (~0 >> ((endIndex % BitsPerByte) +1));
}
