/* $Header$ */

extern List relation_info ARGS((ObjectId relationObjectId));
extern int32 IndexCatalogInformation ARGS((int32 notFirst, ObjectId indrelid, Boolean isarchival, int32 indexCatalogInfo[]));
extern int32 execIndexCatalogInformation ARGS((int32 notFirst, ObjectId indrelid, Boolean isarchival, AttributeNumber indexkeys[]));
extern void IndexSelectivity ARGS((ObjectId indexrelid, ObjectId indrelid, int32 nIndexKeys, ObjectId AccessMethodOperatorClasses[], ObjectId operatorObjectIds[], int32 varAttributeNumbers[], char *constValues[], int32 constFlags[], float32data selectivityInfo[]));
extern float64data restriction_selectivity ARGS((ObjectId functionObjectId, ObjectId operatorObjectId, ObjectId relationObjectId, AttributeNumber attributeNumber, char *constValue, int32 constFlag));
extern float64data join_selectivity ARGS((ObjectId functionObjectId, ObjectId operatorObjectId, ObjectId relationObjectId1, AttributeNumber attributeNumber1, ObjectId relationObjectId2, AttributeNumber attributeNumber2));
extern LispValue InheritanceGetChildren ARGS((LispValue inhparent, LispValue list));
extern LispValue VersionGetParents ARGS((LispValue verrelid, LispValue list));
