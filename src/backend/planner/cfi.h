/* ----------------------------------------------------------------
 *   FILE
 *	cfi.h
 *
 *   DESCRIPTION
 *	prototypes for cfi.c.
 *
 *   NOTES
 *	Automatically generated using mkproto
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#ifndef cfiIncluded		/* include this file only once */
#define cfiIncluded	1

extern List index_info ARGS((bool not_first, ObjectId relid));
extern List index_selectivity ARGS((ObjectId indid, List classes, List opnos, ObjectId relid, List attnos, List values, List flags, int32 nkeys));
extern LispValue find_version_parents ARGS((LispValue relation_oid));
extern int32 function_index_info ARGS((int32 function_oid, int32 index_oid));
extern int32 function_index_info ARGS((int32 function_oid, int32 index_oid));

#endif /* cfiIncluded */
