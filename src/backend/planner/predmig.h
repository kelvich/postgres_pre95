/* ----------------------------------------------------------------
 *   FILE
 *	predmig.h
 *
 *   DESCRIPTION
 *	prototypes for predmig.c.
 *
 *   NOTES
 *	Automatically generated using mkproto
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#ifndef predmigIncluded		/* include this file only once */
#define predmigIncluded	1

extern bool xfunc_do_predmig ARGS((Path root));
extern void xfunc_predmig ARGS((JoinPath pathnode, Stream streamroot, Stream laststream, bool *progressp));
extern bool xfunc_series_llel ARGS((Stream stream));
extern bool xfunc_llel_chains ARGS((Stream root, Stream bottom));
extern Stream xfunc_complete_stream ARGS((Stream stream));
extern bool xfunc_prdmig_pullup ARGS((Stream origstream, Stream pullme, JoinPath joinpath));
extern void xfunc_form_groups ARGS((Stream root, Stream bottom));
extern void xfunc_free_stream ARGS((Stream root));
extern Stream xfunc_add_clauses ARGS((Stream current));
extern void xfunc_setup_group ARGS((Stream node, Stream bottom));
extern Stream xfunc_streaminsert ARGS((CInfo clauseinfo, Stream current, int clausetype));
extern int xfunc_num_relids ARGS((Stream node));
extern StreamPtr xfunc_get_downjoin ARGS((Stream node));
extern StreamPtr xfunc_get_upjoin ARGS((Stream node));
extern Stream xfunc_stream_qsort ARGS((Stream root, Stream bottom));
extern int xfunc_stream_compare ARGS((void *arg1, void *arg2));
extern bool xfunc_check_stream ARGS((Stream node));
extern bool xfunc_in_stream ARGS((Stream node, Stream stream));

#endif /* predmigIncluded */
