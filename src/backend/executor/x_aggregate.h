extern void ExecEndAgg ARGS((Agg node));
extern List ExecInitAgg ARGS((Agg node, EState estate, Plan parent));
extern TupleTableSlot ExecAgg ARGS((Agg node));
extern char *update_aggregate ARGS((char *Caggname, char *running_comp, Datum attr_data, ObjectId func1, ObjectId func2));
extern char *finalize_aggregate ARGS((Datum *running_comp, ObjectId func));

