/* ----------------------------------------------------------------
 *   FILE
 *	ex_utils.h
 *
 *   DESCRIPTION
 *	prototypes for ex_utils.c.
 *
 *   NOTES
 *	Automatically generated using mkproto
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#ifndef ex_utilsIncluded		/* include this file only once */
#define ex_utilsIncluded	1

extern void ResetTupleCount ARGS((void));
extern void DisplayTupleCount ARGS((FILE *statfp));
extern BaseNode ExecGetPrivateState ARGS((Plan node));
extern void ExecAssignNodeBaseInfo ARGS((EState estate, BaseNode basenode, Plan parent));
extern void ExecAssignDebugHooks ARGS((Plan node, BaseNode basenode));
extern void ExecAssignExprContext ARGS((EState estate, CommonState commonstate));
extern void ExecAssignResultType ARGS((CommonState commonstate, ExecTupDescriptor execTupDesc, TupleDescriptor tupDesc));
extern void ExecAssignResultTypeFromOuterPlan ARGS((Plan node, CommonState commonstate));
extern void ExecAssignResultTypeFromTL ARGS((Plan node, CommonState commonstate));
extern TupleDescriptor ExecGetResultType ARGS((CommonState commonstate));
extern void ExecFreeResultType ARGS((CommonState commonstate));
extern void ExecAssignProjectionInfo ARGS((Plan node, CommonState commonstate));
extern void ExecFreeProjectionInfo ARGS((CommonState commonstate));
extern TupleDescriptor ExecGetScanType ARGS((CommonScanState csstate));
extern void ExecFreeScanType ARGS((CommonScanState csstate));
extern void ExecAssignScanType ARGS((CommonScanState csstate, ExecTupDescriptor execTupDesc, TupleDescriptor tupDesc));
extern void ExecAssignScanTypeFromOuterPlan ARGS((Plan node, CommonState commonstate));
extern Attribute ExecGetTypeInfo ARGS((Relation relDesc));
extern TupleDescriptor ExecMakeTypeInfo ARGS((int nelts));
extern void ExecSetTypeInfo ARGS((int index, struct attribute **typeInfo, ObjectId typeID, int attNum, int attLen, char *attName, Boolean attbyVal));
extern ExecTupDescriptor ExecMakeExecTupDesc ARGS((int len));
extern ExecAttDesc ExecMakeExecAttDesc ARGS((AttributeTag tag, int len));
extern ExecAttDesc MakeExecAttDesc ARGS((AttributeTag tag, int len, TupleDescriptor tupdesc));
extern void ExecFreeTypeInfo ARGS((struct attribute **typeInfo));
extern List QueryDescGetTypeInfo ARGS((List queryDesc));
extern List ExecCollect ARGS((List l, List (*applyFunction)(), List (*collectFunction)(), List applyParameters));
extern List ExecUniqueCons ARGS((List list1, List list2));
extern List ExecGetVarAttlistFromExpr ARGS((Node expr, List relationNum));
extern List ExecGetVarAttlistFromTLE ARGS((List tle, List relationNum));
extern AttributeNumberPtr ExecMakeAttsFromList ARGS((List attlist, int *numAttsPtr));
extern void ExecInitScanAttributes ARGS((Plan node));
extern AttributeNumberPtr ExecMakeBogusScanAttributes ARGS((int natts));
extern void ExecFreeScanAttributes ARGS((AttributeNumberPtr ptr));
extern int ExecGetVarLen ARGS((Plan node, CommonState commonstate, Var var));
extern TupleDescriptor ExecGetVarTupDesc ARGS((Plan node, CommonState commonstate, Var var));

#endif /* ex_utilsIncluded */
