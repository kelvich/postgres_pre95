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
extern int AggregateDefine ARGS((Name aggName, Name aggtransfn1Name, Name aggtransfn2Name, Name aggfinalfnName, Name aggbasetypeName, Name aggtransfn1typeName, Name aggtransfn2typeName, String agginitval1, String agginitval2));
extern char *AggNameGetInitVal ARGS((char *aggName, int xfuncno, bool *isNull));

/* prototypes for functions in lib/catalog/pg_operator.c */
extern void OperatorDefine ARGS((Name operatorName, Name leftTypeName, Name rightTypeName, Name procedureName, uint16 precedence, Boolean isLeftAssociative, Name commutatorName, Name negatorName, Name restrictionName, Name joinName, Boolean canHash, Name leftSortName, Name rightSortName));

/* protoypes for functions in lib/catalog/pg_type.c */
extern ObjectId TypeGetWithOpenRelation ARGS((Relation pg_type_desc, Name typeName, bool *defined));
extern ObjectId TypeGet ARGS((Name typeName, bool *defined));
extern ObjectId TypeShellMakeWithOpenRelation ARGS((Relation pg_type_desc, Name typeName));
extern ObjectId TypeShellMake ARGS((Name typeName));
extern ObjectId TypeDefine ARGS((Name typeName, ObjectId relationOid, int16 internalSize, int16 externalSize, int typeType, int typDelim, Name inputProcedure, Name outputProcedure, Name sendProcedure, Name receiveProcedure, Name elementTypeName, char *defaultTypeValue, Boolean passedByValue));
extern void TypeRename ARGS((Name oldTypeName, Name newTypeName));

#endif
