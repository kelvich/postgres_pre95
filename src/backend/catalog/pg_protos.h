/* ------------------------------------------
 *   FILE
 *	pg_protos.h
 * 
 *   DESCRIPTION
 *	prototypes for functions in lib/catalog/pg_operator.c,
 *                                  lib/catalog/pg_type.c,
 *                                  lib/catalog/pg_aggregate.c
 *
 *   IDENTIFICATION
 *	$Header$
 * -------------------------------------------
 */
#ifndef PG_PROTOS_H
#define PG_PROTOS_H

#include "utils/rel.h"

/* prototypes for functions in lib/catalog/pg_aggregate.c */
ObjectId AggregateGetWithOpenRelation ARGS((Relation pg_aggregate_desc , Name aggName , ObjectId int1ObjectId , ObjectId int2ObjectId , ObjectId finObjectId ));ObjectId AggregateGet ARGS((Name aggName , Name int1funcName , Name int2funcName , Name finfuncName ));
int AggregateDefine ARGS((Name aggName , Name xitionfunc1Name , Name xitionfunc2Name , Name finalfuncName , int initaggval , int initsecval ));
char *AggNameGetInitVal ARGS((char *aggName , int initValAttno , bool *isNull ));

/*	prototypes for functions in lib/catalog/pg_operator.c */
ObjectId OperatorGetWithOpenRelation ARGS((Relation pg_operator_desc , Name operatorName , ObjectId leftObjectId , ObjectId rightObjectId ));
ObjectId OperatorGet ARGS((Name operatorName , Name leftTypeName , Name rightTypeName ));
ObjectId OperatorShellMakeWithOpenRelation ARGS((Relation pg_operator_desc , Name operatorName , ObjectId leftObjectId , ObjectId rightObjectId ));
ObjectId OperatorShellMake ARGS((Name operatorName , Name leftTypeName , Name rightTypeName ));
int OperatorDef ARGS((Name operatorName , int definedOK , Name leftTypeName , Name rightTypeName , Name procedureName , uint16 precedence , Boolean isLeftAssociative , Name commutatorName , Name negatorName , Name restrictionName , Name joinName , Boolean canHash , Name leftSortName , Name rightSortName ));
int OperatorUpd ARGS((ObjectId baseId , ObjectId commId , ObjectId negId ));
void OperatorDefine ARGS((Name operatorName , Name leftTypeName , Name rightTypeName , Name procedureName , uint16 precedence , Boolean isLeftAssociative , Name commutatorName , Name negatorName , Name restrictionName , Name joinName , Boolean canHash , Name leftSortName , Name rightSortName ));
HeapTuple FindDefaultType ARGS((char *operatorName , int leftTypeId ));

/* protoypes for functions in lib/catalog/pg_type.c */
ObjectId TypeGetWithOpenRelation ARGS((Relation pg_type_desc , Name typeName , bool *defined ));
ObjectId TypeGet ARGS((Name typeName , bool *defined ));
ObjectId TypeShellMakeWithOpenRelation ARGS((Relation pg_type_desc , Name typeName ));
ObjectId TypeShellMake ARGS((Name typeName ));
ObjectId TypeDefine ARGS((Name typeName , ObjectId relationOid , int16 internalSize , int16 externalSize , int typeType , int typDelim , Name inputProcedure , Name outputProcedure , Name sendProcedure , Name receiveProcedure , Name elementTypeName , char *defaultTypeValue , Boolean passedByValue ));
void TypeRename ARGS((Name oldTypeName , Name newTypeName ));

#endif
