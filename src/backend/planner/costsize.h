/*$Header$*/
extern Cost cost_seqscan ARGS((LispValue relid, int relpages, int reltuples));
extern Cost cost_index ARGS((ObjectId indexid, int expected_indexpages, int selec, int relpages, int reltuples, int indexpages, int indextuples));
extern Cost cost_sort ARGS((LispValue keys, int tuples, LispValue width, LispValue noread));
extern Cost cost_hash ARGS((LispValue keys, int tuples, LispValue width, LispValue which_rel));
extern Cost cost_result ARGS((int tuples, LispValue width));
extern Cost cost_nestloop ARGS((int outercost, int innercost, int outertuples));
extern Cost cost_mergesort ARGS((int outercost, int innercost, LispValue outersortkeys, LispValue innersortkeys, int outersize, int innersize, LispValue outerwidth, LispValue innerwidth));
extern Cost cost_hashjoin ARGS((int outercost, int innercost, LispValue outerkeys, LispValue innerkeys, LispValue outersize, LispValue innersize, LispValue outerwidth, LispValue innerwidth));
extern int compute_rel_size ARGS((LispValue rel));
extern int compute_rel_width ARGS((LispValue rel));
extern int compute_targetlist_width ARGS((LispValue targetlist));
extern int compute_attribute_width ARGS((LispValue tlistentry));
extern int compute_joinrel_size ARGS((LispValue joinrel));
extern int page_size ARGS((int tuples, int width));
extern double base_log ARGS((double x, double b));
