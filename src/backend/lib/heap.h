/* ------------------------------------------
 *   FILE
 *	heap.h
 * 
 *   DESCRIPTION
 *	prototypes for functions in lib/catalog/heap.c
 *
 *   IDENTIFICATION
 *	$Header$
 * -------------------------------------------
 */
#ifndef HEAP_H
#define HEAP_H
Relation heap_creatr ARGS((char relname [], unsigned natts , unsigned smgr , struct attribute *att []));
void CheckAttributeNames ARGS((unsigned natts , struct attribute *tupdesc []));
int RelationAlreadyExists ARGS((Relation pg_relation_desc , char relname []));
void AddNewAttributeTuples ARGS((ObjectId new_rel_oid , ObjectId new_type_oid , unsigned natts , struct attribute *tupdesc []));
void AddPgRelationTuple ARGS((Relation pg_relation_desc , Relation new_rel_desc , ObjectId new_rel_oid , int arch , unsigned natts ));
ObjectId heap_create ARGS((char relname [], int arch , unsigned natts , unsigned smgr , struct attribute *tupdesc []));
void RelationRemoveInheritance ARGS((Relation relation ));
void RelationRemoveIndexes ARGS((Relation relation ));
void DeletePgRelationTuple ARGS((Relation rdesc ));
void DeletePgAttributeTuples ARGS((Relation rdesc ));
void DeletePgTypeTuple ARGS((Relation rdesc ));
void UnlinkRelationFile ARGS((Relation rdesc ));
void heap_destroy ARGS((char relname []));
#endif
