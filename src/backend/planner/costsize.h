/* ----------------------------------------------------------------
 *   FILE
 *	costsize.h
 *
 *   DESCRIPTION
 *	prototypes for costsize.c.
 *
 *   NOTES
 *	Automatically generated using mkproto
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#ifndef costsizeIncluded		/* include this file only once */
#define costsizeIncluded	1

extern bool _enable_seqscan_;
extern bool _enable_indexscan_;
extern bool _enable_sort_;
extern bool _enable_hash_;
extern bool _enable_nestloop_;
extern bool _enable_mergesort_;
extern bool _enable_hashjoin_;

extern double base_log ARGS((double x, double b));

extern Cost cost_seqscan ARGS((LispValue relid, int relpages, int reltuples));
extern Cost cost_index ARGS((ObjectId indexid, Count expected_indexpages, Cost selec, Count relpages, Count reltuples, Count indexpages, Count indextuples, bool is_injoin));
extern Cost cost_sort ARGS((LispValue keys, int tuples, int width, bool noread));
extern Cost cost_hash ARGS((LispValue keys, int tuples, int width, int which_rel));
extern Cost cost_result ARGS((int tuples, int width));
extern Cost cost_nestloop ARGS((Cost outercost, Cost innercost, Count outertuples, Count innertuples, Count outerpages, bool is_indexjoin));
extern Cost cost_mergesort ARGS((Cost outercost, Cost innercost, LispValue outersortkeys, LispValue innersortkeys, int outersize, int innersize, int outerwidth, int innerwidth));
extern Cost cost_hashjoin ARGS((Cost outercost, Cost innercost, LispValue outerkeys, LispValue innerkeys, int outersize, int innersize, int outerwidth, int innerwidth));
extern int compute_rel_size ARGS((Rel rel));
extern int compute_rel_width ARGS((Rel rel));
extern int compute_targetlist_width ARGS((LispValue targetlist));
extern int compute_attribute_width ARGS((LispValue tlistentry));
extern int compute_joinrel_size ARGS((JoinPath joinpath));
extern int page_size ARGS((int tuples, int width));

#endif /* costsizeIncluded */
