/*
 * iqual.h --
 *	Index scan key qualification definitions.
 *
 * Identification:
 *	$Header$
 */

#ifndef	IQualDefined	/* Include this file only once. */
#define IQualDefined	1

#ifndef C_H
#include "c.h"
#endif

#include "itemid.h"
#include "rel.h"
#include "skey.h"

/*
 * ItemIdSatisfiesScanKey --
 *	Returns true iff item associated with an item identifier satisifes
 *	the index scan key qualification.
 *
 * Note:
 *	Assumes item identifier is valid.
 *	Assumes standard page.
 *	Assumes relation is valid.
 *	Assumes scan key is valid.
 */
extern
bool
ItemIdSatisfiesScanKey ARGS((
	ItemId		itemId,
	Page		page,
	Relation	relation,
	ScanKey		key
));

#endif	/* !defined(IQualDefined) */
