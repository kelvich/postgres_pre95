/* ------------------------------------------
 *   FILE
 *	equalfuncs.h
 * 
 *   DESCRIPTION
 *	prototypes for functions in lib/C/equalfuncs.c
 *
 *   IDENTIFICATION
 *	$Header$
 * -------------------------------------------
 */
#ifndef EQUALFUNCS_H
#define EQUALFUNCS_H

#include "nodes/nodes.h"
#include "nodes/primnodes.h"
#include "nodes/relation.h"
#include "nodes/execnodes.h"
#include "nodes/plannodes.h"

extern bool equal ARGS((Node a , Node b ));
extern bool _equalResdom ARGS((Resdom a , Resdom b ));
extern bool _equalExpr ARGS((Expr a , Expr b ));
extern bool _equalVar ARGS((Var a , Var b ));
extern bool _equalArray ARGS((Array a , Array b ));
extern bool _equalOper ARGS((Oper a , Oper b ));
extern bool _equalConst ARGS((Const a , Const b ));
extern bool _equalParam ARGS((Param a , Param b ));
extern bool _equalFunc ARGS((Func a , Func b ));
extern bool _equalCInfo ARGS((CInfo a , CInfo b ));
extern bool _equalJoinMethod ARGS((JoinMethod a , JoinMethod b ));
extern bool _equalPath ARGS((Path a , Path b ));
extern bool _equalIndexPath ARGS((IndexPath a , IndexPath b ));
extern bool _equalJoinPath ARGS((JoinPath a , JoinPath b ));
extern bool _equalMergePath ARGS((MergePath a , MergePath b ));
extern bool _equalHashPath ARGS((HashPath a , HashPath b ));
extern bool _equalJoinKey ARGS((JoinKey a , JoinKey b ));
extern bool _equalMergeOrder ARGS((MergeOrder a , MergeOrder b ));
extern bool _equalHInfo ARGS((HInfo a , HInfo b ));
extern bool _equalIndexScan ARGS((IndexScan a , IndexScan b ));
extern bool _equalJInfo ARGS((JInfo a , JInfo b ));
extern bool _equalEState ARGS((EState a , EState b ));
extern bool _equalHeapTuple ARGS((HeapTuple a , HeapTuple b ));
extern bool _equalRelation ARGS((Relation a , Relation b ));
extern bool _equalLispValue ARGS((LispValue a , LispValue b ));
extern bool _equalFragment ARGS((Fragment a , Fragment b ));
#endif
