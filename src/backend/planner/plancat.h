/* ----------------------------------------------------------------
 *   FILE
 *	plancat.h
 *
 *   DESCRIPTION
 *	prototypes for plancat.c.
 *
 *   NOTES
 *	Automatically generated using mkproto
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#ifndef plancatIncluded		/* include this file only once */
#define plancatIncluded	1

extern LispValue relation_info ARGS((Index relid));
extern int32 IndexCatalogInformation ARGS((int32 notFirst, ObjectId indrelid, Boolean isarchival, long indexCatalogInfo[]));
extern int32 execIndexCatalogInformation ARGS((int32 notFirst, ObjectId indrelid, Boolean isarchival, AttributeNumber indexkeys[]));
extern void IndexSelectivity ARGS((ObjectId indexrelid, ObjectId indrelid, int32 nIndexKeys, ObjectId AccessMethodOperatorClasses[], ObjectId operatorObjectIds[], int32 varAttributeNumbers[], char *constValues[], int32 constFlags[], float32data selectivityInfo[]));
extern float64data restriction_selectivity ARGS((ObjectId functionObjectId, ObjectId operatorObjectId, ObjectId relationObjectId, AttributeNumber attributeNumber, char *constValue, int32 constFlag));
extern float64data join_selectivity ARGS((ObjectId functionObjectId, ObjectId operatorObjectId, ObjectId relationObjectId1, AttributeNumber attributeNumber1, ObjectId relationObjectId2, AttributeNumber attributeNumber2));
extern LispValue find_inheritance_children ARGS((LispValue inhparent));
extern LispValue VersionGetParents ARGS((LispValue verrelid, LispValue list));

#endif /* plancatIncluded */
