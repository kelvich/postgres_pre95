/* $Header$ */
extern TupleTableSlot SeqNext ARGS((SeqScan node));
extern TupleTableSlot ExecSeqScan ARGS((SeqScan node));
extern ObjectId InitScanRelation ARGS((Plan node, EState estate, int level, ScanState scanstate, Plan outerPlan));
extern List ExecInitSeqScan ARGS((Plan node, EState estate, int level, Plan parent));
extern void ExecEndSeqScan ARGS((SeqScan node));
extern void ExecSeqReScan ARGS((Plan node));
extern List ExecSeqMarkPos ARGS((Plan node));
extern void ExecSeqRestrPos ARGS((Plan node));
