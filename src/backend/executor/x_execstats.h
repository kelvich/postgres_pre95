/* $Header$ */
extern void IncrementRetrieved ARGS((EState estate));
extern void IncrementAppended ARGS((EState estate));
extern void IncrementDeleted ARGS((EState estate));
extern void IncrementReplaced ARGS((EState estate));
extern void IncrementInserted ARGS((EState estate));
extern void IncrementProcessed ARGS((EState estate));
extern void DisplayTupleCount ARGS((EState estate));
