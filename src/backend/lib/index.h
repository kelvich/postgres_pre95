/* ----------------------------------------------------------------
 *   FILE
 *	index.h
 *
 *   DESCRIPTION
 *	prototypes for index.c.
 *
 *   NOTES
 *	Automatically generated using mkproto
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#ifndef indexIncluded		/* include this file only once */
#define indexIncluded	1

#include "access/funcindex.h"
#include "nodes/execnodes.h"

extern int FindIndexNAtt ARGS((int32 first, ObjectId indrelid, Boolean isarchival));
extern ObjectId RelationNameGetObjectId ARGS((Name relationName, Relation pg_relation, bool setHasIndexAttribute));
extern int GetHeapRelationOid ARGS((Name heapRelationName, Name indexRelationName));
extern TupleDescriptor BuildFuncTupleDesc ARGS((FuncIndexInfo *funcInfo));
extern TupleDescriptor ConstructTupleDescriptor ARGS((ObjectId heapoid, Relation heapRelation, AttributeNumber numatts, AttributeNumber attNums[]));
extern AccessMethodTupleForm AccessMethodObjectIdGetAccessMethodTupleForm ARGS((ObjectId accessMethodObjectId));
extern void ConstructIndexReldesc ARGS((Relation indexRelation, ObjectId amoid));
extern ObjectId UpdateRelationRelation ARGS((Relation indexRelation));
extern void InitializeAttributeOids ARGS((Relation indexRelation, AttributeNumber numatts, ObjectId indexoid));
extern void AppendAttributeTuples ARGS((Relation indexRelation, AttributeNumber numatts));
extern void UpdateIndexRelation ARGS((ObjectId indexoid, ObjectId heapoid, FuncIndexInfo *funcInfo, AttributeNumber natts, AttributeNumber attNums[], ObjectId classOids[], LispValue predicate));
extern void UpdateIndexPredicate ARGS((ObjectId indexoid, LispValue oldPred, LispValue predicate));
extern void InitIndexStrategy ARGS((AttributeNumber numatts, Relation indexRelation, ObjectId accessMethodObjectId));
extern void index_create ARGS((Name heapRelationName, Name indexRelationName, FuncIndexInfo *funcInfo, ObjectId accessMethodObjectId, AttributeNumber numatts, AttributeNumber attNums[], ObjectId classObjectId[], uint16 parameterCount, Datum parameter[], LispValue predicate));
extern void index_destroy ARGS((ObjectId indexId));
extern void FormIndexDatum ARGS((AttributeNumber numberOfAttributes, AttributeNumber attributeNumber[], HeapTuple heapTuple, TupleDescriptor heapDescriptor, Buffer buffer, Datum *datum, char *nullv, FuncIndexInfoPtr fInfo));
extern int UpdateStats ARGS((ObjectId relid, long reltuples, bool hasindex));
extern void FillDummyExprContext ARGS((ExprContext econtext, TupleTableSlot slot, TupleDescriptor tupdesc, Buffer buffer));
extern void DefaultBuild ARGS((Relation heapRelation, Relation indexRelation, AttributeNumber numberOfAttributes, AttributeNumber attributeNumber[], IndexStrategy indexStrategy, uint16 parameterCount, Datum parameter[], FuncIndexInfoPtr funcInfo, LispValue predInfo));
extern void index_build ARGS((Relation heapRelation, Relation indexRelation, AttributeNumber numberOfAttributes, AttributeNumber attributeNumber[], uint16 parameterCount, Datum parameter[], FuncIndexInfo *funcInfo, LispValue predInfo));

#endif /* indexIncluded */
