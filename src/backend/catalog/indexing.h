/*
 * This include provides some definitions to support indexing on system
 * catalogs
 *
 * $Header$
 */

#ifndef _INDEXING_H_INCL_
#define _INDEXING_H_INCL_

/*
 * Some definitions for indices on pg_attribute
 */
#define Num_pg_attr_indices	3
#define Num_pg_proc_indices	2
#define Num_pg_type_indices	2
#define Num_pg_name_indices	2
#define Num_pg_class_indices	2


/*
 * Names of indices on system catalogs
 */
extern Name AttributeNameIndex;
extern Name AttributeNumIndex;
extern Name ProcedureNameIndex;
extern Name ProcedureOidIndex;
extern Name TypeNameIndex;
extern Name TypeOidIndex;
extern Name NamingNameIndex;
extern Name NamingParentIndex;
extern Name ClassNameIndex;
extern Name ClassOidIndex;

extern char *Name_pg_attr_indices[];
extern char *Name_pg_proc_indices[];
extern char *Name_pg_type_indices[];
extern char *Name_pg_name_indices[];
extern char *Name_pg_class_indices[];

extern char *IndexedCatalogName[];

/*
 * Functions for each index to perform the necessary scan on a cache miss.
 */
HeapTuple AttributeNameIndexScan ARGS((Relation heapRelation , ObjectId relid , char *relname ));
HeapTuple AttributeNumIndexScan  ARGS((Relation heapRelation , ObjectId relid , AttributeNumber relname ));

HeapTuple ProcedureOidIndexScan  ARGS((Relation heapRelation, ObjectId procId));
HeapTuple ProcedureNameIndexScan ARGS((Relation heapRelation, char *procName));

HeapTuple TypeOidIndexScan  ARGS((Relation heapRelation, ObjectId typeId));
HeapTuple TypeNameIndexScan ARGS((Relation heapRelation, char *typeName));

HeapTuple NamingNameIndexScan ARGS((Relation heapRelation, ObjectId parentid, char *filename));

HeapTuple ClassNameIndexScan ARGS((Relation heapRelation, char *relName));
HeapTuple ClassOidIndexScan ARGS((Relation heapRelation, ObjectId relId));

/*
 * What follows are lines processed by genbki.sh to create the statements
 * the bootstrap parser will turn into DefineIndex commands.
 *
 * The keyword is DECLARE_INDEX every thing after that is just like in a
 * normal specification of the 'define index' POSTQUEL command.
 */
DECLARE_INDEX(pg_attnameind on pg_attribute using btree (mkoidchar16(attrelid, attname) oidchar16_ops));
DECLARE_INDEX(pg_attnumind  on pg_attribute using btree (mkoidint4(attrelid, attnum) oidint4_ops));
DECLARE_INDEX(pg_attrelidind on pg_attribute using btree (attrelid oid_ops))

DECLARE_INDEX(pg_procidind on pg_proc using btree (oid oid_ops));
DECLARE_INDEX(pg_procnameind on pg_proc using btree (proname char16_ops));

DECLARE_INDEX(pg_typeidind on pg_type using btree (oid oid_ops));
DECLARE_INDEX(pg_typenameind on pg_type using btree (typname char16_ops));

DECLARE_INDEX(pg_namingind on pg_naming using btree (mkoidchar16(parentid, filename) oidchar16_ops));
DECLARE_INDEX(pg_namparind on pg_naming using btree (parentid oid_ops));

DECLARE_INDEX(pg_classnameind on pg_class using btree (relname char16_ops));
DECLARE_INDEX(pg_classoidind on pg_class using btree (oid oid_ops));

/* now build indices in the initialization scripts */
BUILD_INDICES

/* indexing.c prototypes */

void CatalogOpenIndices ARGS((int nIndices, char *names [], Relation idescs[]));
void CatalogCloseIndices ARGS((int nIndices , Relation *idescs ));
void CatalogIndexInsert ARGS((Relation *idescs , int nIndices , Relation heapRelation , HeapTuple heapTuple ));
bool CatalogHasIndex ARGS((char *catName , ObjectId catId ));

#endif /* _INDEXING_H_INCL_ */
