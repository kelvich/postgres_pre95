
; /*
; * 
; * POSTGRES Data Base Management System
; * 
; * Copyright (c) 1988 Regents of the University of California
; * 
; * Permission to use, copy, modify, and distribute this software and its
; * documentation for educational, research, and non-profit purposes and
; * without fee is hereby granted, provided that the above copyright
; * notice appear in all copies and that both that copyright notice and
; * this permission notice appear in supporting documentation, and that
; * the name of the University of California not be used in advertising
; * or publicity pertaining to distribution of the software without
; * specific, written prior permission.  Permission to incorporate this
; * software into commercial products can be obtained from the Campus
; * Software Office, 295 Evans Hall, University of California, Berkeley,
; * Ca., 94720 provided only that the the requestor give the University
; * of California a free licence to any derived software for educational
; * and research purposes.  The University of California makes no
; * representations about the suitability of this software for any
; * purpose.  It is provided "as is" without express or implied warranty.
; * 
; */



/*
 * relcache.h --
 *	Relation descriptor cache definitions.
 *
 * Identification:
 *	$Header$
 */

#ifndef	RelCacheIncluded	/* Include this file only once. */
#define RelCacheIncluded	1

#include "c.h"

#include "oid.h"
#include "rel.h"

/*
 * RelationFree --
 *	doesn't do anything.  Ever.
 */
extern
void
RelationFree ARGS((
	Relation	relation
));

/*
 * Relation Name Get Relation--
 *	Returns the matching relation descriptor 
 */
extern Relation
getreldesc ARGS((
	Name		relationName
));

/*
 * XXXX
 *
 * getreldesc is an obsolete name.  Use RelationNameGetRelation instead
 * in all cases.
 *
 */

#define RelationNameGetRelation(name) getreldesc(name)

/*
 * Relation Id Get Relation--
 *	Returns the matching relation descriptor 
 */
extern Relation
RelationIdGetRelation ARGS((
	ObjectId	relationId
));

#ifdef EXTERN_UNDEFINED_FUNCTIONS
/*
 * RelationAllocate --
 *	Allocates a relation descriptor.
 */
extern
Relation
RelationAllocate ARGS((
	void	/* XXX ??? */
));
#endif

/*
 * XXXX
 *
 * The following two functions should be private to relcache.c but
 * arn't.
 *
 */

extern File
relopen ARGS((
	char	relationName[];
	int	flags;
	int	mode
));

extern void
RelationRegisterRelation ARGS((
	Relation	relation
));

/*
 * RelationFlushRelation
 *
 *   Actually blows away a relation... RelationFree doesn't do 
 *   anything anymore.
 */

extern void
RelationFlushRelation ARGS((
	Relation        relation;
	bool            onlyFlushReferenceCountZero
));

/*
 * RelationIdInvalidateRelationCacheByRelationId --
 */
extern
void
RelationIdInvalidateRelationCacheByRelationId ARGS((
	ObjectId	relationId;
));

/*
 * RelationIdInvalidateRelationCacheByAccessMethodId --
 */
extern
void
RelationIdInvalidateRelationCacheByAccessMethodId ARGS((
	ObjectId	accessMethodId;
));

/*
 * InvalidateRelationCacheIndexes --
 */
extern
void
InvalidateRelationCacheIndexes ARGS((
	void
));

/*
 * RelationCacheInvalidate
 *
 *   Will blow away either all the cached relation descriptors or
 *   those that have a zero reference count.
 *
 */

extern void
RelationCacheInvalidate ARGS((
	bool            onlyFlushReferenceCountZero
));

#endif	/* !defined(RelCacheIncluded) */
