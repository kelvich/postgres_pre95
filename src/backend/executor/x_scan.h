/* $Header$ */
extern List ExecScan ARGS((Plan node, int accessMtd));
extern List ExecScanTemp ARGS((ScanTemp node));
extern List ExecInitScanTemp ARGS((ScanTemp node, EState estate, int level));
extern void ExecEndScanTemp ARGS((ScanTemp node));
