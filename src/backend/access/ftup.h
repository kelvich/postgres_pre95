/* ----------------------------------------------------------------
 *   FILE
 *	ftup.h
 *
 *   XXX these macros are obsolete -- use the index_ routines
 *	 directly.  This file is going away..  -cim 4/30/91
 *
 *   DESCRIPTION
 *	POSTGRES definitions for tuple formation and modification.
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#ifndef	FTupIncluded	/* Include this file only once. */
#define FTupIncluded	1

#include "tmp/c.h"

#include "access/att.h"
#include "access/attnum.h"
#include "access/itup.h"
#include "access/htup.h"
#include "access/tupdesc.h"

/* ----------------
 *	old macros
 * ----------------
 */
/*
 * FormHeapTuple --
 *	Returns a palloc'd heap tuple.
 */
#define FormHeapTuple(numberOfAttributes, tupleDescriptor, value, nulls) \
    heap_formtuple(numberOfAttributes, tupleDescriptor, value, nulls)

#define formtuple(numberOfAttributes, tupleDescriptor, value, nulls) \
    heap_formtuple(numberOfAttributes, tupleDescriptor, value, nulls)

/*
 * FormIndexTuple --
 *	Returns a palloc'd heap tuple.
 */
#define FormIndexTuple(numberOfAttributes, tupleDescriptor, value, nulls) \
    index_formtuple(numberOfAttributes, tupleDescriptor, value, nulls)

#define formituple(numberOfAttributes, tupleDescriptor, value, nulls) \
    index_formtuple(numberOfAttributes, tupleDescriptor, value, nulls)

/*
 * ModifyHeapTuple --
 *	Returns a palloc'd heap tuple.
 *
 * Note:
 *	Assumes the tuple is valid.
 *	Assumes either the buffer or relation is valid.
 *	For now, assumes replaceValue, replaceNull, and replace are
 *	fully specified.
 */
#define ModifyHeapTuple(tuple, buffer, relation, replValue, replNull, repl) \
    heap_modifytuple(tuple, buffer, relation, replValue, replNull, repl)

#define modifytuple(tuple, buffer, relation, replValue, replNull, repl) \
    heap_modifytuple(tuple, buffer, relation, replValue, replNull, repl)

/* 
 * addtupleheader
 */
#define addtupleheader(natts, structlen, structure) \
    heap_addheader(natts, structlen, structure)

#endif	/* !defined(FTupIncluded) */
