/* $Header$ */
extern List SeqNext ARGS((SeqScan node));
extern List ExecSeqScan ARGS((SeqScan node));
extern List ExecInitSeqScan ARGS((Plan node, EState estate, int level));
extern void ExecEndSeqScan ARGS((SeqScan node));
extern void ExecSeqReScan ARGS((Plan node));
extern List ExecSeqMarkPos ARGS((Plan node));
extern void ExecSeqRestrPos ARGS((Plan node, List pos));
