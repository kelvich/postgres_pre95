/* ------------------------------------------
 *   FILE
 *	index.h
 * 
 *   DESCRIPTION
 *	prototypes for functions in lib/catalog/index.c
 *
 *   IDENTIFICATION
 *	$Header$
 * -------------------------------------------
 */
#ifndef INDEX_H
#define INDEX_H
int FindIndexNAtt ARGS((int32 first , ObjectId indrelid , Boolean isarchival ));
ObjectId RelationNameGetObjectId ARGS((Name relationName , Relation pg_relation , bool setHasIndexAttribute ));
int GetHeapRelationOid ARGS((Name heapRelationName , Name indexRelationName ));
TupleDescriptor BuildFuncTupleDesc ARGS((FuncIndexInfo *funcInfo ));
TupleDescriptor ConstructTupleDescriptor ARGS((ObjectId heapoid , Relation heapRelation , AttributeNumber numatts , AttributeNumber attNums []));
AccessMethodTupleForm AccessMethodObjectIdGetAccessMethodTupleForm ARGS((ObjectId accessMethodObjectId ));
void ConstructIndexReldesc ARGS((Relation indexRelation , ObjectId amoid ));
ObjectId UpdateRelationRelation ARGS((Relation indexRelation ));
void InitializeAttributeOids ARGS((Relation indexRelation , AttributeNumber numatts , ObjectId indexoid ));
void AppendAttributeTuples ARGS((Relation indexRelation , AttributeNumber numatts ));
void UpdateIndexRelation ARGS((ObjectId indexoid , ObjectId heapoid , FuncIndexInfo *funcInfo , AttributeNumber natts , AttributeNumber attNums [], ObjectId classOids []));
void InitIndexStrategy ARGS((AttributeNumber numatts , Relation indexRelation , ObjectId accessMethodObjectId ));
void index_create ARGS((Name heapRelationName , Name indexRelationName , FuncIndexInfo *funcInfo , ObjectId accessMethodObjectId , AttributeNumber numatts , AttributeNumber attNums [], ObjectId classObjectId [], uint16 parameterCount , Datum parameter [], LispValue predicate ));
void index_destroy ARGS((ObjectId indexId ));
void FormIndexDatum ARGS((AttributeNumber numberOfAttributes , AttributeNumber attributeNumber [], HeapTuple heapTuple , TupleDescriptor heapDescriptor , Buffer buffer , Datum *datum , char *null , FuncIndexInfoPtr fInfo ));
int UpdateStats ARGS((Relation whichRel , long reltuples ));
void FillDummyExprContext ARGS((ExprContext econtext , TupleTableSlot slot , TupleDescriptor tupdesc , Buffer buffer ));
void DefaultBuild ARGS((Relation heapRelation , Relation indexRelation , AttributeNumber numberOfAttributes , AttributeNumber attributeNumber [], IndexStrategy indexStrategy , uint16 parameterCount , Datum parameter [], FuncIndexInfoPtr funcInfo , LispValue predicate ));
void index_build ARGS((Relation heapRelation , Relation indexRelation , AttributeNumber numberOfAttributes , AttributeNumber attributeNumber [], uint16 parameterCount , Datum parameter [], FuncIndexInfo *funcInfo , LispValue predicate ));
#endif
