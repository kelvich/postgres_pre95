/* $Header$ */
extern void ExecOpenScanR ARGS((ObjectId relOid, int nkeys, ScanKey skeys, bool isindex, ScanDirection dir, TimeQual timeRange, Relation *returnRelation, Pointer *returnScanDesc));
extern Relation ExecOpenR ARGS((ObjectId relationOid, bool isindex));
extern Pointer ExecBeginScan ARGS((Relation relation, int nkeys, ScanKey skeys, bool isindex, ScanDirection dir, TimeQual time_range));
extern void ExecCloseR ARGS((Plan node));
extern void ExecReScan ARGS((Plan node));
extern HeapScanDesc ExecReScanR ARGS((Relation relDesc, HeapScanDesc scanDesc, ScanDirection direction, int nkeys, ScanKey skeys));
extern List ExecMarkPos ARGS((Plan node));
extern void ExecRestrPos ARGS((Plan node, List pos));
extern Relation ExecCreatR ARGS((int numberAttributes, AttributePtr tupType, int relationOid));
extern void ExecDestroyR ARGS((List relID));
