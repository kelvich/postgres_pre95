/* $Header$ */
extern List ExecScan ARGS((Plan node, int accessMtd));
extern List ExecScanTemps ARGS((ScanTemps node));
extern List ExecInitScanTemps ARGS((ScanTemps node, EState estate, int level));
extern void ExecEndScanTemps ARGS((ScanTemps node));
