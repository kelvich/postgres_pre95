/* $Header$ */
extern List donothing ARGS((List tuple, List attrdesc));
extern List ExecMain ARGS((List queryDesc, EState estate, List feature));
extern List InitPlan ARGS((int operation, List parseTree, Plan plan, EState estate));
extern List EndPlan ARGS((Plan plan, EState estate));
extern List ExecutePlan ARGS((EState estate, Plan plan, int operation, int numberTuples, int direction, int printfunc));
extern List ExecRetrieve ARGS((HeapTuple tuple, AttributePtr attrtype, int printfunc, Relation intoRelationDesc));
extern List ExecAppend ARGS((HeapTuple tuple, ItemPointer tupleid, EState estate));
extern List ExecDelete ARGS((HeapTuple tuple, ItemPointer tupleid, EState estate));
extern List ExecReplace ARGS((HeapTuple tuple, ItemPointer tupleid, EState estate));
