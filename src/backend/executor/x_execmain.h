/* $Header$ */
extern List donothing ARGS((List tuple, List attrdesc));
extern List ExecMain ARGS((List queryDesc, EState estate, List feature));
extern List InitPlan ARGS((int operation, List parseTree, Plan plan, EState estate));
extern List EndPlan ARGS((Plan plan, EState estate));
extern TupleTableSlot ExecutePlan ARGS((EState estate, Plan plan, LispValue parseTree, int operation, int numberTuples, int direction, int printfunc));
extern TupleTableSlot ExecRetrieve ARGS((TupleTableSlot slot, AttributePtr attrtype, int printfunc, Relation intoRelationDesc));
extern TupleTableSlot ExecAppend ARGS((TupleTableSlot slot, ItemPointer tupleid, EState estate));
extern TupleTableSlot ExecDelete ARGS((TupleTableSlot slot, ItemPointer tupleid, EState estate));
extern TupleTableSlot ExecReplace ARGS((TupleTableSlot slot, ItemPointer tupleid, EState estate, LispValue parseTree));
