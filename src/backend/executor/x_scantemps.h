/* ----------------------------------------------------------------
 *   FILE
 *	n_scantemps.h
 *
 *   DESCRIPTION
 *	prototypes for n_scantemps.c.
 *
 *   NOTES
 *	Automatically generated using mkproto
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#ifndef n_scantempsIncluded		/* include this file only once */
#define n_scantempsIncluded	1

extern TupleTableSlot ExecScanTemps ARGS((ScanTemps node));
extern List ExecInitScanTemps ARGS((ScanTemps node, EState estate, Plan parent));
extern int ExecCountSlotsScanTemps ARGS((Plan node));
extern void ExecEndScanTemps ARGS((ScanTemps node));

#endif /* n_scantempsIncluded */
