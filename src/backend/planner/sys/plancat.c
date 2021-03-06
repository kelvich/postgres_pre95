/*
 * plancat.c --
 *	C routines needed by the planner (e.g., routines for accessing the
 *	system catalogs).
 */

#include <stdio.h>
#include "tmp/postgres.h"

RcsId("$Header$");

#include "access/heapam.h"
#include "access/htup.h"
#include "access/itup.h"
#include "access/tqual.h"
#include "parser/parsetree.h"
#include "fmgr.h"
#include "utils/log.h"

#include "nodes/pg_lisp.h"

#include "catalog/catname.h"
#include "catalog/syscache.h"
#include "catalog/pg_amop.h"
#include "catalog/pg_index.h"
#include "catalog/pg_inherits.h"
#include "catalog/pg_version.h"

/*
 *	RelationCatalogInformation
 *
 *	Given the relid of a relation, retrieve into the array
 *	'relationCatalogInfo' the following information:
 *		whether the relation has secondary indices
 *		number of pages 
 *		number of tuples
 * 
 *    	relation-info
 *    
 *    	Retrieves catalog information for a given relation.
 *    
 *    	Returns a list containing relhasindex, relpages, and reltuples.
 *    
 */

/*  .. get_rel
 */

LispValue
relation_info (relid)
	Index	relid;
{
	HeapTuple		relationTuple;
	RelationTupleForm	relation;
	int32			rel_info[3];
	LispValue		retval = LispNil;
	int 			i;
	ObjectId 		relationObjectId;

	extern	LispValue _query_range_table_;

	relationObjectId = 
	  (ObjectId)CInteger(getrelid ( relid, _query_range_table_ ));
	relationTuple = SearchSysCacheTuple(RELOID, (char *) relationObjectId,
					    (char *) NULL, (char *) NULL,
					    (char *) NULL);
	if (HeapTupleIsValid(relationTuple)) {
		relation = (RelationTupleForm)GETSTRUCT(relationTuple);
		rel_info[0] = (relation->relhasindex) ? 1 : 0;
		rel_info[1] = relation->relpages;
		rel_info[2] = relation->reltuples;
	} else
		elog(WARN, "RelationCatalogInformation: Relation %d not found",
		     relationObjectId);
	for(i=2; i>=0 ; --i) 
	    retval = lispCons(lispInteger(rel_info[i]),retval);
	return(retval);

}

/* 
 *	IndexCatalogInformation
 *
 * 	Returns index info in array 'indexCatalogInfo' in the following order:
 *		OID of the index relation
 *			(NOT the OID of the relation being indexed)
 *		the number of pages in the index relation
 *		the number of tuples in the index relation
 *		the 8 keys of the index
 *		the 8 ordering operators of the index
 *		the 8 classes of the AM operators of the index
 *		TOTAL: 27 long words.
 *
 *	On the first call to this routine, the index relation is opened.
 *	Successive calls will return subsequent indices until no more
 *	are found.
 *
 *	Returns 1 if an index was found, and 0 otherwise.
 */

int32
IndexCatalogInformation(notFirst, indrelid, isarchival, indexCatalogInfo)
	int32		notFirst;
	ObjectId	indrelid;		/* indexED relation */
	Boolean		isarchival;		/* XXX not used YET */
        long		indexCatalogInfo[];
{
	register		i;
	HeapTuple		indexTuple, amopTuple;
	IndexTupleForm		index;
	Relation		indexRelation;
	uint16			amstrategy;	/* XXX not used YET */
	ObjectId		relam;
	LispValue		predicate;
	static Relation		relation = (Relation) NULL;
	static HeapScanDesc	scan = (HeapScanDesc) NULL;
	static ScanKeyEntryData	indexKey[1] = {
		{ 0, IndexHeapRelationIdAttributeNumber, F_OIDEQ }
	};

	fmgr_info(F_OIDEQ, &indexKey[0].func, &indexKey[0].nargs);

	/* 29 = 27 (see above) + 1 (indproc) + 1 (indpred) */
	bzero((char *) indexCatalogInfo, (unsigned) (29 * sizeof(long)));

	/* Find an index on the given relation */
	if (notFirst == 0) {
		if (RelationIsValid(relation))
			RelationCloseHeapRelation(relation);
		if (HeapScanIsValid(scan))
			HeapScanEnd(scan);
		indexKey[0].argument = ObjectIdGetDatum(indrelid);
		relation = RelationNameOpenHeapRelation(IndexRelationName);
		scan = RelationBeginHeapScan(relation, 0, NowTimeQual,
					     1, (ScanKey) indexKey);
	}
	if (!HeapScanIsValid(scan))
		elog(WARN, "IndexCatalogInformation: scan not started");
	indexTuple = HeapScanGetNextTuple(scan, 0, (Buffer *) NULL);
	if (!HeapTupleIsValid(indexTuple)) {
		HeapScanEnd(scan);
		RelationCloseHeapRelation(relation);
		scan = (HeapScanDesc) NULL;
		relation = (Relation) NULL;
		return(0);
	}

	/* Extract info from the index tuple */
	index = (IndexTupleForm)GETSTRUCT(indexTuple);
	indexCatalogInfo[0] = index->indexrelid;	/* index relation */
	for (i = 0; i < 8; ++i)					/* 3-10 */
		indexCatalogInfo[i+3] = index->indkey[i];
	for (i = 0; i < 8; ++i)					/* 19-26 */
		indexCatalogInfo[i+19] = index->indclass[i];
	
	/* functional index ?? */
	indexCatalogInfo[27] = index->indproc;			/* 27 */

	/* partial index ?? */
	predicate = LispNil;
	if (VARSIZE(&index->indpred) != 0) {
	    /*
	     * The memory allocated here for the predicate (in lispReadString)
	     * only needs to stay around until it's used in find_index_paths,
	     * which is all within a command, so the automatic pfree at end
	     * of transaction should be ok.
	     */
	    char *predString;
	    LispValue lispReadString();
	    predString = fmgr(F_TEXTOUT, &index->indpred);
	    predicate = lispReadString(predString);
	    pfree(predString);
	}
	indexCatalogInfo[28] = (long) predicate;		/* 28 */

	/* Extract info from the relation descriptor for the index */
	indexRelation = (Relation) index_open(index->indexrelid);
#ifdef notdef
	/* XXX should iterate through strategies -- but how?  use #1 for now */
	amstrategy = indexRelation->rd_am->amstrategies;
#endif notdef
	amstrategy = 1;
	relam = indexRelation->rd_rel->relam;
	indexCatalogInfo[1] = indexRelation->rd_rel->relpages;
	indexCatalogInfo[2] = indexRelation->rd_rel->reltuples;
	RelationCloseHeapRelation(indexRelation);
	
	/* 
	 * Find the index ordering keys 
	 *
	 * Must use indclass to know when to stop looking since with
	 * functional indices there could be several keys (args) for
	 * one opclass. -mer 27 Sept 1991
	 */
	for (i = 0; i < 8 && index->indclass[i]; ++i) {		/* 11-18 */
		amopTuple = SearchSysCacheTuple(AMOPSTRATEGY,
						(char *) relam,
						(char *) index->indclass[i],
						(char *) amstrategy,
						(char *) NULL);
		if (!HeapTupleIsValid(amopTuple))
			elog(WARN, "IndexCatalogInformation: no amop %d %d %d",
			     relam, index->indclass[i], amstrategy);
		indexCatalogInfo[i+11] = ((AccessMethodOperatorTupleForm)
			GETSTRUCT(amopTuple))->amopoprid;
	}
	return(1);
}

/*
 *	execIndexCatalogInformation
 *
 *	Returns the index relid for each index on the relation with OID 
 *	'indrelid'.  'indexkeys' is loaded with the index key values.
 */
int32
execIndexCatalogInformation(notFirst, indrelid, isarchival, indexkeys)
	int32		notFirst;
	ObjectId	indrelid;		/* indexED relation */
	Boolean		isarchival;		/* XXX not used YET */
	AttributeNumber	indexkeys[];
{
	register int	i;
	int32		found, indexCatalogInfo[27];

	found = IndexCatalogInformation(notFirst, indrelid, isarchival,
					indexCatalogInfo);
	if (! found || indexCatalogInfo[0] == 0)
		return((int32) InvalidObjectId);
	for (i = 0; i < MaxIndexAttributeNumber; ++i)
		indexkeys[i] = indexCatalogInfo[i+3];
	return((int32) indexCatalogInfo[0]);	
}

/*
 *	IndexSelectivity
 *
 *	Retrieves the 'amopnpages' and 'amopselect' parameters for each
 *	AM operator when a given index (specified by 'indexrelid') is used.
 *	These two parameters are returned by copying them to into an array of
 *	floats.
 *
 *	Assumption: the attribute numbers and operator ObjectIds are in order
 *	WRT to each other (otherwise, you have no way of knowing which
 *	AM operator class or attribute number corresponds to which operator.
 *
 *	'varAttributeNumbers' contains attribute numbers for variables
 *	'constValues' contains the constant values
 *	'constFlags' describes how to treat the constants in each clause
 *	'nIndexKeys' describes how many keys the index actually has
 *
 *	Returns 'selectivityInfo' filled with the sum of all pages touched
 *	and the product of each clause's selectivity.
 */
/*ARGSUSED*/
void
IndexSelectivity(indexrelid, indrelid, nIndexKeys,
		 AccessMethodOperatorClasses, operatorObjectIds,
		 varAttributeNumbers, constValues, constFlags,
		 selectivityInfo)
	ObjectId	indexrelid;
	ObjectId	indrelid;
	int32		nIndexKeys;
	ObjectId	AccessMethodOperatorClasses[];	/* XXX not used? */
	ObjectId	operatorObjectIds[];
	int32		varAttributeNumbers[];
	char		*constValues[];
	int32		constFlags[];
	float32data	selectivityInfo[];
{
	register	i, n;
	HeapTuple	indexTuple, amopTuple;
	IndexTupleForm	index;
	AccessMethodOperatorTupleForm	amop;
	ObjectId	indclass;
	float64data	npages, select;
	float64		amopnpages, amopselect;

	indexTuple = SearchSysCacheTuple(INDEXRELID,
					 (char *) indexrelid, (char *) NULL,
					 (char *) NULL, (char *) NULL);
	if (!HeapTupleIsValid(indexTuple))
		elog(WARN, "IndexSelectivity: index %d not found",
		     indexrelid);
	index = (IndexTupleForm)GETSTRUCT(indexTuple);

	npages = 0.0;
	select = 1.0;
	for (n = 0; n < nIndexKeys; ++n) {
		/* 
		 * Find the AM class for this key. 
		 *
		 * If the first attribute number is invalid then we have a
		 * functional index, and AM class is the first one defined
		 * since functional indices have exactly one key.
		 */
		indclass = (varAttributeNumbers[0] == InvalidAttributeNumber) ?
			index->indclass[0] : InvalidObjectId;
		i = 0;
		while ((i < nIndexKeys) && (indclass == InvalidObjectId)) {
			if (varAttributeNumbers[n] == index->indkey[i]) {
				indclass = index->indclass[i];
				break;
			}
			i++;
		}
		if (!ObjectIdIsValid(indclass)) {
			/*
			 * Presumably this means that we are using a functional
			 * index clause and so had no variable to match to
			 * the index key ... if not we are in trouble.
			 */
			elog(NOTICE, "IndexSelectivity: no key %d in index %d",
			     varAttributeNumbers[n], indexrelid);
			continue;
		}

		amopTuple = SearchSysCacheTuple(AMOPOPID,
						(char *) indclass,
						(char *) operatorObjectIds[n],
						(char *) NULL, (char *) NULL);
		if (!HeapTupleIsValid(amopTuple))
			elog(WARN, "IndexSelectivity: no amop %d %d",
			     indclass, operatorObjectIds[n]);
		amop = (AccessMethodOperatorTupleForm)GETSTRUCT(amopTuple);
		amopnpages = (float64) fmgr(amop->amopnpages,
					    (char *) operatorObjectIds[n],
					    (char *) indrelid,
					    (char *) varAttributeNumbers[n],
					    (char *) constValues[n],
					    (char *) constFlags[n],
					    (char *) nIndexKeys, 
					    (char *) indexrelid);
		npages += PointerIsValid(amopnpages) ? *amopnpages : 0.0;
		if ((i = npages) < npages)	/* ceil(npages)? */
			npages += 1.0;
		amopselect = (float64) fmgr(amop->amopselect,
					    (char *) operatorObjectIds[n],
					    (char *) indrelid,
					    (char *) varAttributeNumbers[n],
					    (char *) constValues[n],
					    (char *) constFlags[n],
					    (char *) nIndexKeys,
					    (char *) indexrelid);
		select *= PointerIsValid(amopselect) ? *amopselect : 1.0;
	}
	selectivityInfo[0] = npages;
	selectivityInfo[1] = select;
}

/*
 *      restriction_selectivity in lisp system.
 *	NOTE:  used to be called RestrictionClauseSelectivity in C
 *
 *	Returns the selectivity of a specified operator.
 *	This code executes registered procedures stored in the
 *	operator relation, by calling the function manager.
 *
 *	XXX The assumption in the selectivity procedures is that if the
 *		relation OIDs or attribute numbers are -1, then the clause
 *		isn't of the form (op var const).
 */
float64data
restriction_selectivity(functionObjectId, operatorObjectId,
			relationObjectId, attributeNumber,
			constValue, constFlag)
	ObjectId	functionObjectId;
	ObjectId	operatorObjectId;
	ObjectId	relationObjectId;
	AttributeNumber	attributeNumber;
	char		*constValue;
	int32		constFlag;
{
	float64	result;

	result = (float64) fmgr(functionObjectId,
				(char *) operatorObjectId,
				(char *) relationObjectId, 
				(char *) attributeNumber,
				(char *) constValue,
				(char *) constFlag);
	if (!PointerIsValid(result))
		elog(WARN, "RestrictionClauseSelectivity: bad pointer");
	if (*result < 0.0 || *result > 1.0)
		elog(WARN, "RestrictionClauseSelectivity: bad value %lf",
		     *result);
	return(*result);
}

/*
 *      join_selectivity in lisp system.
 *	used to be called JoinClauseSelectivity in C
 *
 *	Returns the selectivity of a join operator.
 *
 *	XXX The assumption in the selectivity procedures is that if the
 *		relation OIDs or attribute numbers are -1, then the clause
 *		isn't of the form (op var var).
 */
float64data
join_selectivity (functionObjectId, operatorObjectId,
		  relationObjectId1, attributeNumber1,
		  relationObjectId2, attributeNumber2)
     ObjectId	functionObjectId;
     ObjectId	operatorObjectId;
     ObjectId	relationObjectId1;
     AttributeNumber	attributeNumber1;
     ObjectId 	relationObjectId2;
     AttributeNumber	attributeNumber2;
{
    float64	result = 0;
    
    result = (float64) fmgr(functionObjectId,
			    (char *) operatorObjectId,
			    (char *) relationObjectId1,
			    (char *) attributeNumber1,
			    (char *) relationObjectId2,
			    (char *) attributeNumber2);
    if (!PointerIsValid(result))
      elog(WARN, "JoinClauseSelectivity: bad pointer");
    if (*result < 0.0 || *result > 1.0)
      elog(WARN, "JoinClauseSelectivity: bad value %lf",
	   *result);
    return(*result);
}

/*
 *	find_all_inheritors
 *
 *	Returns a LISP list containing the OIDs of all relations which
 *	inherits from the relation with OID 'inhparent'.
 */
LispValue
find_inheritance_children (inhparent)
     LispValue	inhparent;
{
	HeapTuple		inheritsTuple;
	static ScanKeyEntryData	key[1] = {
		{ 0, InheritsParentAttributeNumber, F_OIDEQ }
	};
	Relation		relation;
	HeapScanDesc		scan;
	LispValue		lp = LispNil;
	LispValue		list = LispNil;

	fmgr_info(F_OIDEQ, &key[0].func, &key[0].nargs);

	key[0].argument = ObjectIdGetDatum((ObjectId) LISPVALUE_INTEGER(inhparent));
	relation = RelationNameOpenHeapRelation(InheritsRelationName);
	scan = RelationBeginHeapScan(relation, 0, NowTimeQual,
				     1, (ScanKey) key);
	while (HeapTupleIsValid(inheritsTuple =
				HeapScanGetNextTuple(scan, 0,
						     (Buffer *) NULL))) {
	     lp = lispInteger(((InheritsTupleForm)
			       GETSTRUCT(inheritsTuple))->inhrel);
	     list = nappend1(list,lp);
	}
	HeapScanEnd(scan);
	RelationCloseHeapRelation(relation);
	return(list);
   }

/*
 *	VersionGetParents
 *
 *	Returns a LISP list containing the OIDs of all relations which are
 *	base relations of the relation with OID 'verrelid'.
 */
LispValue
VersionGetParents(verrelid, list)
	LispValue	verrelid, list;
{
	HeapTuple		versionTuple;
	static ScanKeyEntryData	key[1] = {
		{ 0, VersionRelationIdAttributeNumber, F_OIDEQ }
	};
	Relation		relation;
	HeapScanDesc		scan;
	LispValue		lp;
	ObjectId		verbaseid;

	fmgr_info(F_OIDEQ, &key[0].func, &key[0].nargs);
	relation = RelationNameOpenHeapRelation(VersionRelationName);
	key[0].argument = ObjectIdGetDatum((ObjectId) LISPVALUE_INTEGER(verrelid));
	scan = RelationBeginHeapScan(relation, 0, NowTimeQual,
				     1, (ScanKey) key);
	for (;;) {
		versionTuple = HeapScanGetNextTuple(scan, 0,
						    (Buffer *) NULL);
		if (!HeapTupleIsValid(versionTuple))
			break;
		verbaseid = ((VersionTupleForm)
			     GETSTRUCT(versionTuple))->verbaseid;
		lp = lispList();
		CAR(lp) = lispInteger(verbaseid);
		CDR(lp) = CAR(list);
		CAR(list) = lp;
		key[0].argument = ObjectIdGetDatum(verbaseid);
		HeapScanRestart(scan, 0, (ScanKey) key);
	}
	HeapScanEnd(scan);
	RelationCloseHeapRelation(relation);
	return(list);
}
