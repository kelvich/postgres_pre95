/*
 * tupdesc.h --
 *	POSTGRES tuple descriptor definitions.
 *
 * Identification:
 *	$Header$
 */

#ifndef	TupDescIncluded	/* Include this file only once. */
#define TupDescIncluded	1

#ifndef C_H
#include "c.h"
#endif

#include "att.h"

typedef struct TupleDescriptorData {
	AttributeTupleForm	data[1];	/* VARIABLE LENGTH ARRAY */
} TupleDescriptorData;

typedef TupleDescriptorData	*TupleDescriptor;

#endif	/* !defined(TupDescIncluded) */
