/* $Header$ */
extern void ExecIncrementRetrieved ARGS((EState estate));
extern void ExecIncrementAppended ARGS((EState estate));
extern void ExecIncrementDeleted ARGS((EState estate));
extern void ExecIncrementReplaced ARGS((EState estate));
extern void ExecIncrementInserted ARGS((EState estate));
extern void ExecIncrementProcessed ARGS((EState estate));
extern void DisplayTupleCount ARGS((EState estate));
