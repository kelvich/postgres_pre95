/* $Header$ */
extern void ExecMergeFreeOuterTuple ARGS((ExprContext econtext));
extern void ExecMergeFreeInnerTuple ARGS((MergeJoinState mergestate, ExprContext econtext));
extern List MJFormOSortopI ARGS((List qualList, ObjectId sortOp));
extern List MJFormISortopO ARGS((List qualList, ObjectId sortOp));
extern bool MergeCompare ARGS((List eqQual, List compareQual, ExprContext econtext));
extern void ExecMergeTupleDump ARGS((ExprContext econtext, MergeJoinState mergestate));
extern List ExecMergeJoin ARGS((MergeJoin node));
extern List ExecInitMergeJoin ARGS((MergeJoin node, EState estate, int level));
extern void ExecEndMergeJoin ARGS((MergeJoin node));
