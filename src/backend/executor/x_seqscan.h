/* ----------------------------------------------------------------
 *   FILE
 *	n_seqscan.h
 *
 *   DESCRIPTION
 *	prototypes for n_seqscan.c.
 *
 *   NOTES
 *	Automatically generated using mkproto
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#ifndef n_seqscanIncluded		/* include this file only once */
#define n_seqscanIncluded	1

extern TupleTableSlot SeqNext ARGS((SeqScan node));
extern TupleTableSlot ExecSeqScan ARGS((SeqScan node));
extern ObjectId InitScanRelation ARGS((Plan node, EState estate, ScanState scanstate, Plan outerPlan));
extern List ExecInitSeqScan ARGS((Plan node, EState estate, Plan parent));
extern int ExecCountSlotsSeqScan ARGS((Plan node));
extern void ExecEndSeqScan ARGS((SeqScan node));
extern void ExecSeqReScan ARGS((Plan node));
extern List ExecSeqMarkPos ARGS((Plan node));
extern void ExecSeqRestrPos ARGS((Plan node));

#endif /* n_seqscanIncluded */
