/* ------------------------------------------
 *   FILE
 *	copyfuncs.h
 * 
 *   DESCRIPTION
 *	prototypes for functions in lib/C/copyfuncs.h
 *
 *   IDENTIFICATION
 *	$Header$
 * -------------------------------------------
 */
#ifndef COPYFUNCSINCLUDED
#define COPYFUNCSINCLUDED
/* copyfuncs.c */
LispValue lispCopy ARGS((LispValue lispObject ));
bool NodeCopy ARGS((Node from , Node *toP , char *(*alloc )()));
Node CopyObject ARGS((Node from ));
Node CopyObjectUsing ARGS((Node from , char *(*alloc )()));
bool CopyNodeFields ARGS((Node from , Node newnode , char *(*alloc )()));
bool CopyPlanFields ARGS((Plan from , Plan newnode , char *(*alloc )()));
bool CopyScanFields ARGS((Scan from , Scan newnode , char *(*alloc )()));
Relation CopyRelDescUsing ARGS((Relation reldesc , char *(*alloc )()));
List copyRelDescsUsing ARGS((List relDescs , char *(*alloc )()));
bool CopyScanTempFields ARGS((ScanTemps from , ScanTemps newnode , char *(*alloc )()));
bool CopyJoinFields ARGS((Join from , Join newnode , char *(*alloc )()));
bool CopyTempFields ARGS((Temp from , Temp newnode , char *(*alloc )()));
bool CopyExprFields ARGS((Expr from , Expr newnode , char *(*alloc )()));
bool CopyPathFields ARGS((Path from , Path newnode , char *(*alloc )()));
bool CopyJoinPathFields ARGS((JoinPath from , JoinPath newnode , char *(*alloc )()));
bool CopyJoinMethodFields ARGS((JoinMethod from , JoinMethod newnode , char *(*alloc )()));
#endif
