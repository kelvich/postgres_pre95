/*
 * bit.c --
 *	Standard bit array code.
 *
 * Identification:
 *	$Header$
 */

#include "utils/memutils.h"

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
