/*=========================================================================
 * FILE:
 *	junk.c
 *
 * $Header$
 *
 * DESCRIPTION:
 *	 Junk attribute support stuff....
 *
 *=========================================================================
 */

#include "tmp/postgres.h"
#include "parser/parsetree.h"
#include "utils/log.h"
#include "nodes/pg_lisp.h"
#include "nodes/plannodes.h"
#include "nodes/plannodes.a.h"
#include "nodes/primnodes.h"
#include "nodes/primnodes.a.h"
#include "nodes/execnodes.h"
#include "nodes/execnodes.a.h"
#include "access/ftup.h"
#include "access/heapam.h"
#include "executor/execdebug.h"

#include "executor/externs.h"

extern Node CopyObject();
extern List MakeTLE();

/*-------------------------------------------------------------------------
 * 	XXX this stuff should be rewritten to take advantage
 * 	    of ExecProject() and the ProjectionInfo node.
 * 	    -cim 6/3/91
 * 
 * An attribute of a tuple living inside the executor, can be
 * either a normal attribute or a "junk" attribute. "junk" attributes
 * never make it out of the executor, i.e. they are never printed,
 * returned or stored in disk. Their only purpose in life is to
 * store some information useful only to the executor, mainly the values
 * of some system attributes like "ctid" or rule locks.
 * 
 * The general idea is the following: A target list consists of a list of
 * Resdom nodes & expression pairs. Each Resdom node has an attribute
 * called 'resjunk'. If the value of this attribute is 1 then the
 * corresponding attribute is a "junk" attribute.
 * 
 * When we initialize a plan  we call 'ExecInitJunkFilter' to create
 * and store the appropriate information in the 'es_junkFilter' attribute of
 * EState.
 * 
 * We then execute the plan ignoring the "resjunk" attributes.
 * 
 * Finally, when at the top level we get back a tuple, we can call
 * 'ExecGetJunkAttribute' to retrieve the value of the junk attributes we
 * are interested in, and 'ExecRemoveJunk' to remove all the junk attributes
 * from a tuple. This new "clean" tuple is then printed, replaced, deleted
 * or inserted.
 * 
 *-------------------------------------------------------------------------
 */

/*-------------------------------------------------------------------------
 * ExecInitJunkFilter
 *
 * Initialize the Junk filter.
 *-------------------------------------------------------------------------
 */
JunkFilter
ExecInitJunkFilter(targetList)
    List targetList;
{
    JunkFilter 		junkfilter;
    List 		cleanTargetList;
    int 		len, cleanLength;
    TupleDescriptor 	tupType, cleanTupType;
    List 		t, expr, tle;
    Resdom 		resdom, cleanResdom;
    int 		resjunk;
    AttributeNumber 	resno, cleanResno;
    AttributeNumberPtr 	cleanMap;
    Size 		size;

    /* ---------------------
     * First find the "clean" target list, i.e. all the entries
     * in the original target list which have a zero 'resjunk'
     * NOTE: make copy of the Resdom nodes, because we have
     * to change the 'resno's...
     * ---------------------
     */
    cleanTargetList = LispNil;
    cleanResno = 1;
    
    foreach (t, targetList) {
	resdom = (Resdom) tl_resdom(CAR(t));
	expr = tl_expr(CAR(t));
	resjunk = get_resjunk(resdom);
	if (resjunk == 0) {
	    /*
	     * make a copy of the resdom node, changing its resno.
	     */
	    cleanResdom = (Resdom) CopyObject(resdom);
	    set_resno(cleanResdom, cleanResno);
	    cleanResno ++;
	    /*
	     * create a new target list entry
	     */
	    tle = (List) MakeTLE(cleanResdom, expr);
	    cleanTargetList = nappend1(cleanTargetList, tle);
	}
    }

    /* ---------------------
     * Now calculate the tuple types for the original and the clean tuple
     *
     * XXX ExecTypeFromTL should be used sparingly.  Don't we already
     *	   have the tupType corresponding to the targetlist we are passed?
     *     -cim 5/31/91
     * ---------------------
     */
    tupType = 	   (TupleDescriptor) ExecTypeFromTL(targetList);
    cleanTupType = (TupleDescriptor) ExecTypeFromTL(cleanTargetList);
    
    len = 	  length(targetList);
    cleanLength = length(cleanTargetList);

    /* ---------------------
     * Now calculate the "map" between the original tuples attributes
     * and the "clean" tuple's attributes.
     *
     * The "map" is an array of "cleanLength" attribute numbers, i.e.
     * one entry for every attribute of the "clean" tuple.
     * The value of this entry is the attribute number of the corresponding
     * attribute of the "original" tuple.
     * ---------------------
     */
    if (cleanLength > 0) {
	size = cleanLength * sizeof(AttributeNumber);
	cleanMap = (AttributeNumberPtr) palloc(size);
	cleanResno = 1;
	foreach (t, targetList) {
	    resdom = (Resdom) tl_resdom(CAR(t));
	    expr = tl_expr(CAR(t));
	    resjunk = get_resjunk(resdom);
	    if (resjunk == 0) {
		cleanMap[cleanResno-1] = get_resno(resdom);;
		cleanResno ++;
	    }
	}
    } else {
	cleanMap = NULL;
    }
	
    /* ---------------------
     * Finally create and initialize the JunkFilter.
     * ---------------------
     */
    junkfilter = (JunkFilter) RMakeJunkFilter();

    set_jf_targetList(junkfilter, targetList);
    set_jf_length(junkfilter, len);
    set_jf_tupType(junkfilter, tupType);
    set_jf_cleanTargetList(junkfilter, cleanTargetList);
    set_jf_cleanLength(junkfilter, cleanLength);
    set_jf_cleanTupType(junkfilter, cleanTupType);
    set_jf_cleanMap(junkfilter, cleanMap);

    return(junkfilter);
    
}

/*-------------------------------------------------------------------------
 * ExecGetJunkAttribute
 *
 * Given a tuple (slot), the junk filter and a junk attribute's name,
 * extract & return the value of this attribute.
 *
 * It returns false iff no junk attribute with such name was found.
 *
 * NOTE: isNull might be NULL !
 *-------------------------------------------------------------------------
 */
bool
ExecGetJunkAttribute(junkfilter, slot, attrName, value, isNull)
    JunkFilter 		junkfilter;
    TupleTableSlot 	slot;
    Name 		attrName;
    Datum 		*value;
    Boolean 		*isNull;
{
    List 		targetList;
    List 		t;
    Resdom 		resdom;
    AttributeNumber 	resno;
    Name 		resname;
    int 		resjunk;
    TupleDescriptor 	tupType;
    HeapTuple 		tuple;

    /* ---------------------
     * first look in the junkfilter's target list for
     * an attribute with the given name
     * ---------------------
     */
    resno = 	 InvalidAttributeNumber;
    targetList = get_jf_targetList(junkfilter);
    
    foreach (t, targetList) {
	resdom = (Resdom) tl_resdom(CAR(t));
	resname = get_resname(resdom);
	resjunk = get_resjunk(resdom);
	if (resjunk != 0 && NameIsEqual(resname, attrName)) {
	    /* We found it ! */
	    resno = get_resno(resdom);
	    break;
	}
    }

    if (resno == InvalidAttributeNumber) {
	/* Ooops! We couldn't find this attribute... */
	return(false);
    }

    /* ---------------------
     * Now extract the attribute value from the tuple.
     * ---------------------
     */
    tuple = 	(HeapTuple) ExecFetchTuple(slot);
    tupType = 	(TupleDescriptor) get_jf_tupType(junkfilter);
    
    *value = 	(Datum)
	heap_getattr(tuple, InvalidBuffer, resno, tupType, isNull);

    return true;
}

/*-------------------------------------------------------------------------
 * ExecRemoveJunk
 *
 * Construct and return a tuple with all the junk attributes removed.
 *-------------------------------------------------------------------------
 */
HeapTuple
ExecRemoveJunk(junkfilter, slot)
    JunkFilter junkfilter;
    TupleTableSlot slot;
{
    HeapTuple 		tuple;
    HeapTuple 		cleanTuple;
    AttributeNumberPtr 	cleanMap;
    TupleDescriptor 	cleanTupType;
    TupleDescriptor	tupType;
    int 		cleanLength;
    Boolean 		isNull;
    int 		i;
    Size 		size;
    Datum 		*values;
    char		*nulls;
    Datum		values_array[64];
    char		nulls_array[64];

    /* ----------------
     *	get info from the slot and the junk filter
     * ----------------
     */
    tuple = (HeapTuple) ExecFetchTuple(slot);
    
    tupType = 		(TupleDescriptor) get_jf_tupType(junkfilter);
    cleanTupType = 	(TupleDescriptor) get_jf_cleanTupType(junkfilter);
    cleanLength = 	get_jf_cleanLength(junkfilter);
    cleanMap = 		get_jf_cleanMap(junkfilter);

    /* ---------------------
     *  Handle the trivial case first.
     * ---------------------
     */
    if (cleanLength == 0)
	return (HeapTuple) NULL;

    /* ---------------------
     * Create the arrays that will hold the attribute values
     * and the null information for the new "clean" tuple.
     *
     * Note: we use memory on the stack to optimize things when
     *       we are dealing with a small number of tuples.
     *	     for large tuples we just use palloc.
     * ---------------------
     */
    if (cleanLength > 64) {
	size = 	 cleanLength * sizeof(Datum);
	values = (Datum *) palloc(size);
    
	size = 	 cleanLength * sizeof(char);
	nulls =  (char *) palloc(size);
    } else {
	values = values_array;
	nulls =  nulls_array;
    }
    
    /* ---------------------
     * Exctract one by one all the values of the "clean" tuple.
     * ---------------------
     */
    for (i=0; i<cleanLength; i++) {
	Datum d = (Datum)
	    heap_getattr(tuple, InvalidBuffer, cleanMap[i], tupType, &isNull);
	
	values[i] = d;
	
	if (isNull)
	    nulls[i] = 'n';
	else
	    nulls[i] = ' ';
    }

    /* ---------------------
     * Now form the new tuple.
     * ---------------------
     */
    cleanTuple = heap_formtuple(cleanLength,
				cleanTupType,
				values,
				nulls);
    
    /* ---------------------
     * We are done.  Free any space allocated for 'values' and 'nulls'
     * and return the new tuple.
     * ---------------------
     */
    if (cleanLength > 64) {
	pfree(values);
	pfree(nulls);
    }
    
    return(cleanTuple);
}

