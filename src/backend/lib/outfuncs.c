/*
 * FILE: outfuncs.c
 *
 * $Header$
 *
 * Every node in POSTGRES has an "out" routine associated with it which knows
 * how to create its "ascii" representation. This ascii representation can
 * be used to either print the node (for debugging purposes) or to store it
 * as a string (like the tuple-level rule system does).
 *
 * NOTE: these functions return nothing (i.e. they are declared as "void")
 * but they update the in/out argument of type StringInfo passed to them.
 * This argument contains the string holding the ASCII representation plus
 * some other information (string length, etc.)
 * 
 */
#include <stdio.h>
#include "tmp/postgres.h"

RcsId("$Header$");

#include "access/heapam.h"
#include "access/htup.h"
#include "catalog/syscache.h"
#include "utils/fmgr.h"
#include "utils/log.h"

#include "nodes/nodes.h"
#include "nodes/execnodes.h"
#include "nodes/pg_lisp.h"
#include "nodes/plannodes.h"
#include "nodes/primnodes.h"
#include "nodes/relation.h"

#include "catalog/pg_type.h"
#include "tmp/stringinfo.h"

void _outDatum();

/*
 * print the basic stuff of all nodes that inherit from Plan
 */
void
_outPlanInfo(str, node)
	StringInfo str;
	Plan node;
{
	char buf[500];

	sprintf(buf, " :cost %g", node->cost );
	appendStringInfo(str,buf);
	sprintf(buf, " :size %d", node->plan_size);
	appendStringInfo(str,buf);
	sprintf(buf, " :width %d", node->plan_width);
	appendStringInfo(str,buf);
	sprintf(buf, " :fragment %d", node->fragment);
	appendStringInfo(str,buf);
	sprintf(buf, " :parallel %d", node->parallel);
	appendStringInfo(str,buf);
	sprintf(buf, " :state %s", (node->state == (struct EState *) NULL ?
				"nil" : "non-NIL"));
	appendStringInfo(str,buf);
	sprintf(buf, " :qptargetlist ");
	appendStringInfo(str,buf);
	_outLispValue(str, node->qptargetlist);
	sprintf(buf, " :qpqual ");
	appendStringInfo(str,buf);
	_outLispValue(str, node->qpqual);
	sprintf(buf, " :lefttree ");
	appendStringInfo(str,buf);
	_outLispValue(str, (LispValue)(node->lefttree));
	sprintf(buf, " :righttree ");
	appendStringInfo(str,buf); 
	_outLispValue(str, (LispValue)(node->righttree));

}

/*
 *  Stuff from plannodes.h
 */
void
_outPlan(str, node)
	StringInfo str;
	Plan node;
{
	char buf[500];

	sprintf(buf, "plan");
	appendStringInfo(str,buf);
	_outPlanInfo(str, (Plan) node);

}

void
_outFragment(str, node)
	StringInfo str;
	Fragment node;
{
	char buf[500];

	sprintf(buf, "fragment");
	appendStringInfo(str,buf);
	sprintf(buf, " :frag_root ");
	appendStringInfo(str,buf);
	_outLispValue(str, (LispValue)(node->frag_root));
	sprintf(buf, " :frag_parallel %d", node->frag_parallel);
	appendStringInfo(str,buf);
	sprintf(buf, " :frag_subtrees ");
	appendStringInfo(str,buf);
	_outLispValue(str, (LispValue)(node->frag_subtrees));

}

void
_outResult(str, node)
	StringInfo str;
	Result	node;
{
	char buf[500];

	sprintf(buf, "result");
	appendStringInfo(str,buf);
	_outPlanInfo(str, (Plan) node);

	sprintf(buf, " :resrellevelqual ");
	appendStringInfo(str,buf);
	_outLispValue(str, node->resrellevelqual);
	sprintf(buf, " :resconstantqual ");
	appendStringInfo(str,buf);
	_outLispValue(str, node->resconstantqual);

}

/*
 *  Existential is a subclass of Plan.
 */

void
_outExistential(str, node)
	StringInfo str;
	Existential	node;
{
	char buf[500];

	sprintf(buf, "existential");
	appendStringInfo(str,buf);
	_outPlanInfo(str, (Plan) node);


}

/*
 *  Append is a subclass of Plan.
 */

_outAppend(str, node)
	StringInfo str;
	Append	node;
{
	char buf[500];

	sprintf(buf, "append");
	appendStringInfo(str,buf);
	_outPlanInfo(str, (Plan) node);

	sprintf(buf, " :unionplans ");
	appendStringInfo(str,buf);
	_outLispValue(str, node->unionplans);

	sprintf(buf, " :unionrelid %d", node->unionrelid);
	appendStringInfo(str,buf);

	sprintf(buf, " :unionrtentries ");
	appendStringInfo(str,buf);
	_outLispValue(str, node->unionrtentries);

}

/*
 *  JoinRuleInfo is a subclass of Node
 */
void
_outJoinRuleInfo(str, node)
	StringInfo str;
	JoinRuleInfo node;
{
	char buf[500];

	char *s;

	sprintf(buf, "joinruleinfo");
	appendStringInfo(str,buf);
	sprintf(buf, " :operator %ld", node->jri_operator);
	appendStringInfo(str,buf);
	sprintf(buf, " :inattrno %hd", node->jri_inattrno);
	appendStringInfo(str,buf);
	sprintf(buf, " :outattrno %hd", node->jri_outattrno);
	appendStringInfo(str,buf);
	/*
	 * Note: we print the rule lock inside double quotes, so
	 * that is is easier to read it back...
	 */
	s = RuleLockToString(node->jri_lock);
	sprintf(buf, " :lock \"%s\"", s);
	appendStringInfo(str,buf);
	sprintf(buf, " :ruleid %ld", node->jri_ruleid);
	appendStringInfo(str,buf);
	sprintf(buf, " :stubid %d", node->jri_stubid);
	appendStringInfo(str,buf);

	/*
	 * NOTE: node->stub is only used temprarily, so its
	 * actual value is of no interest. We only print it
	 * for debugging purposes.
	 */
	sprintf(buf, " :stub 0x%xd", node->jri_stub);
	appendStringInfo(str,buf);

}



/*
 *  Join is a subclass of Plan
 */

void
_outJoin(str, node)
	StringInfo str;
	Join	node;
{
	char buf[500];

	sprintf(buf, "join");
	appendStringInfo(str,buf);
	_outPlanInfo(str, (Plan) node);

	sprintf(buf, " :ruleinfo ");
	appendStringInfo(str,buf);
	_outLispValue(str, (LispValue)(node->ruleinfo));


}

/*
 *  NestLoop is a subclass of Join
 */

void
_outNestLoop(str, node)
	StringInfo str;
	NestLoop	node;
{
	char buf[500];

	sprintf(buf, "nestloop");
	appendStringInfo(str,buf);
	_outPlanInfo(str, (Plan) node);

	sprintf(buf, " :ruleinfo ");
	appendStringInfo(str,buf);
	_outLispValue(str, (LispValue)(node->ruleinfo));

}

/*
 *  MergeJoin is a subclass of Join
 */

void
_outMergeJoin(str, node)
	StringInfo str;
	MergeJoin	node;
{
	char buf[500];
	LispValue x;

	sprintf(buf, "mergejoin");
	appendStringInfo(str,buf);
	_outPlanInfo(str, (Plan) node);

	sprintf(buf, " :ruleinfo ");
	appendStringInfo(str,buf);
	_outLispValue(str, (LispValue)(node->ruleinfo));

	sprintf(buf, " :mergeclauses ");
	appendStringInfo(str,buf);
	_outLispValue(str, (LispValue)(node->mergeclauses));

	sprintf(buf, " :mergesortop %ld", node->mergesortop);
	appendStringInfo(str,buf);

	sprintf(buf, " :mergerightorder (");
	appendStringInfo(str, buf);
	foreach (x, node->mergerightorder) {
	    sprintf(buf, "%ld ", CAR(x));
	    appendStringInfo(str, buf);
	  } 
	sprintf(buf, ")");
	appendStringInfo(str, buf);

	sprintf(buf, " :mergeleftorder (");
	appendStringInfo(str, buf);
	foreach (x, node->mergeleftorder) {
	    sprintf(buf, "%ld ", CAR(x));
	    appendStringInfo(str, buf);
	  } 
	sprintf(buf, ")");
	appendStringInfo(str, buf);

}

/*
 *  HashJoin is a subclass of Join.
 */

void
_outHashJoin(str, node)
	StringInfo str;
	HashJoin	node;
{
	char buf[500];

	sprintf(buf, "hashjoin");
	appendStringInfo(str,buf);
	_outPlanInfo(str, (Plan) node);

	sprintf(buf, " :ruleinfo ");
	appendStringInfo(str,buf);
	_outLispValue(str, (LispValue)(node->ruleinfo));

	sprintf(buf, " :hashclauses ");
	appendStringInfo(str,buf);
	_outLispValue(str, (LispValue)(node->hashclauses));

	sprintf(buf, " :hashjoinop %d",node->hashjoinop);
	appendStringInfo(str,buf);
	sprintf(buf, " :hashjointable 0x%x", node->hashjointable);
	appendStringInfo(str,buf);
	sprintf(buf, " :hashjointablekey %d", node->hashjointablekey);
	appendStringInfo(str,buf);
	sprintf(buf, " :hashjointablesize %d", node->hashjointablesize);
	appendStringInfo(str,buf);
	sprintf(buf, " :hashdone %d", node->hashdone);
	appendStringInfo(str,buf);


}

/*
 *  Scan is a subclass of Node
 */

void
_outScan(str, node)
	StringInfo str;
	Scan	node;
{
	char buf[500];

	sprintf(buf, "scan");
	appendStringInfo(str,buf);
	_outPlanInfo(str, (Plan) node);

	sprintf(buf, " :scanrelid %d", node->scanrelid);
	appendStringInfo(str,buf);

}

void
_outScanTemps(str, node)
	StringInfo str;
	ScanTemps node;
{
	char buf[500];

	sprintf(buf, "scantemp");
	appendStringInfo(str,buf);
	sprintf(buf, " :temprelDesc ");
	appendStringInfo(str,buf);
	_outLispValue(str, (LispValue)(node->temprelDescs));

}

/*
 *  SeqScan is a subclass of Scan
 */

void
_outSeqScan(str, node)
	StringInfo str;
	SeqScan	node;
{
	char buf[500];

	sprintf(buf, "seqscan");
	appendStringInfo(str,buf);
	_outPlanInfo(str, (Plan) node);

	sprintf(buf, " :scanrelid %d", node->scanrelid);
	appendStringInfo(str,buf);
	

}

/*
 *  IndexScan is a subclass of Scan
 */

void
_outIndexScan(str, node)
	StringInfo str;
	IndexScan	node;
{
	char buf[500];

	sprintf(buf, "indexscan");
	appendStringInfo(str,buf);
	_outPlanInfo(str, (Plan) node);

	sprintf(buf, " :scanrelid %d", node->scanrelid);
	appendStringInfo(str,buf);

	sprintf(buf, " :indxid ");
	appendStringInfo(str,buf);
	_outLispValue(str, node->indxid);

	sprintf(buf, " :indxqual ");
	appendStringInfo(str,buf);
	_outLispValue(str, node->indxqual);

}

/*
 *  Temp is a subclass of Plan
 */

void
_outTemp(str, node)
	StringInfo str;
	Temp	node;
{
	char buf[500];

	sprintf(buf, "temp");
	appendStringInfo(str,buf);
	_outPlanInfo(str, (Plan) node);

	sprintf(buf, " :tempid %ld", node->tempid);
	appendStringInfo(str,buf);
	sprintf(buf, " :keycount %d", node->keycount);
	appendStringInfo(str,buf);

}

/*
 *  Sort is a subclass of Temp
 */
void
_outSort(str, node)
	StringInfo str;
	Sort	node;
{
	char buf[500];

	sprintf(buf, "sort");
	appendStringInfo(str,buf);
	_outPlanInfo(str, (Plan) node);

	sprintf(buf, " :tempid %ld", node->tempid);
	appendStringInfo(str,buf);
	sprintf(buf, " :keycount %d", node->keycount);
	appendStringInfo(str,buf);

}

void
_outAgg(str, node)
	StringInfo str;
	Agg node;
{
	char buf[500];
	sprintf(buf, "agg");
	appendStringInfo(str,buf);
	_outPlanInfo(str,(Plan)node);

	sprintf(buf, " :tempid %ld", node->tempid);
	appendStringInfo(str, buf);
	sprintf(buf, " :keycount %d", node->keycount);
	appendStringInfo(str,buf);
}


/*
 *  For some reason, unique is a subclass of Temp.
 */

void
_outUnique(str, node)
	StringInfo str;
	Unique node;
{
	char buf[500];

        sprintf(buf, "unique");
	appendStringInfo(str,buf);
	_outPlanInfo(str, (Plan) node);

	sprintf(buf, " :tempid %ld", node->tempid);
	appendStringInfo(str,buf);
	sprintf(buf, " :keycount %d", node->keycount);
	appendStringInfo(str,buf);

}


/*
 *  Hash is a subclass of Temp
 */

void
_outHash(str, node)
	StringInfo str;
	Hash	node;
{
	char buf[500];

	sprintf(buf, "hash");
	appendStringInfo(str,buf);
	_outPlanInfo(str, (Plan) node);

	sprintf(buf, " :hashkey ");
	appendStringInfo(str,buf);
	_outLispValue(str, (LispValue)(node->hashkey));

	sprintf(buf, " :hashtable 0x%x", node->hashtable);
	appendStringInfo(str,buf);
	sprintf(buf, " :hashtablekey %d", node->hashtablekey);
	appendStringInfo(str,buf);
	sprintf(buf, " :hashtablesize %d", node->hashtablesize);
	appendStringInfo(str,buf);


}
/*
 *  Stuff from primnodes.h.
*/

/*
 *  Resdom is a subclass of Node
 */

void
_outResdom(str, node)
	StringInfo str;
	Resdom	node;
{
	char buf[500];

	sprintf(buf, "resdom");
	appendStringInfo(str,buf);
	sprintf(buf, " :resno %hd", node->resno);
	appendStringInfo(str,buf);
	sprintf(buf, " :restype %ld", node->restype);
	appendStringInfo(str,buf);
	sprintf(buf, " :reslen %d", node->reslen);
	appendStringInfo(str,buf);
	sprintf(buf, " :resname \"%s\"",
	       ((node->resname) ? ((char *) node->resname) : "null"));
	appendStringInfo(str,buf);
	sprintf(buf, " :reskey %d", node->reskey);
	appendStringInfo(str,buf);
	sprintf(buf, " :reskeyop %ld", (long int) node->reskeyop);
	appendStringInfo(str,buf);
	sprintf(buf, " :resjunk %d", node->resjunk);
	appendStringInfo(str,buf);

}

void
_outFjoin(str, node)
	StringInfo str;
	Fjoin	node;
{
	char buf[500];
	char *s;
	int i;

	sprintf(buf, "fjoin");
	appendStringInfo(str,buf);
	sprintf(buf, " :initialized %c", node->fj_initialized ? "true":"nil");
	appendStringInfo(str,buf);
	sprintf(buf, " :nNodes %d", node->fj_nNodes);
	appendStringInfo(str,buf);
	s = lispOut(node->fj_innerNode);
	appendStringInfo(str," :innerNode ");
	appendStringInfo(str,s);
	pfree(s);
	appendStringInfo( str, " :results (" );	/* balance the extra ) */
	for (i = 0; i++; i<node->fj_nNodes)
	{
	    sprintf(buf, " %s ", node->fj_results[i] ? "true" : "nil");
	    appendStringInfo(str, buf);
	}
	/* balance the extra ( */
	appendStringInfo( str, ") :alwaysdone (" ); /* balance the extra ) */
	for (i = 0; i++; i<node->fj_nNodes)
	{
	    sprintf(buf, " %s ", ((node->fj_alwaysDone[i]) ? "true" : "nil"));
	    appendStringInfo(str, buf);
	}
	/* balance the extra ( */
	appendStringInfo(str,")");
}
/*
 *  Expr is a subclass of Node
 */

void
_outExpr(str, node)
	StringInfo str;
	Expr	node;
{
	char buf[500];

	sprintf(buf, "expr)");
	appendStringInfo(str,buf);

}

/*
 *  Var is a subclass of Expr
 */

void
_outVar(str, node)
	StringInfo str;
	Var node;
{
	char buf[500];

	sprintf(buf, "var");
	appendStringInfo(str,buf);
	sprintf(buf, " :varno %d", node->varno);
	appendStringInfo(str,buf);
	sprintf(buf, " :varattno %hd", node->varattno);
	appendStringInfo(str,buf);
	sprintf(buf, " :vartype %ld", node->vartype);
	appendStringInfo(str,buf);
	sprintf(buf, " :varid ");
	appendStringInfo(str,buf);

	_outLispValue(str, node->varid);
}

/*
 *  Const is a subclass of Expr
 */

void
_outConst(str, node)
	StringInfo str;
	Const	node;
{
	char buf[500];

	sprintf(buf, "const");
	appendStringInfo(str,buf);
	sprintf(buf, " :consttype %ld", node->consttype);
	appendStringInfo(str,buf);
	sprintf(buf, " :constlen %hd", node->constlen);
	appendStringInfo(str,buf);
	sprintf(buf, " :constisnull %s", (node->constisnull ? "true" : "nil"));
	appendStringInfo(str,buf);
	sprintf(buf, " :constvalue ");
	appendStringInfo(str,buf);
	if (node->constisnull) {
	    sprintf(buf, "NIL ");
	    appendStringInfo(str,buf);
	} else {
	    _outDatum(str, node->constvalue, node->consttype);
	}
	sprintf(buf, " :constbyval %s", (node->constbyval ? "true" : "nil"));
	appendStringInfo(str,buf);

}

/*
 *  Array is a subclass of Expr
 */

void
_outArray(str, node)
	StringInfo str;
	Array	node;
{
	char buf[500];
	sprintf(buf, "array");
	appendStringInfo(str, buf);
	sprintf(buf, " :arrayelemtype %d", node->arrayelemtype);
	appendStringInfo(str, buf);
	sprintf(buf, " :arrayelemlength %d", node->arrayelemlength);
	appendStringInfo(str, buf);
	sprintf(buf, " :arrayelembyval %c", (node->arrayelembyval) ? 't' : 'f');
	appendStringInfo(str, buf);
	sprintf(buf, " :arraylow %d", node->arraylow);
	appendStringInfo(str, buf);
	sprintf(buf, " :arrayhigh %d", node->arrayhigh);
	appendStringInfo(str, buf);
	sprintf(buf, " :arraylen %d", node->arraylen);
	appendStringInfo(str, buf);
}

/*
 *  ArrayRef is a subclass of Expr
 */

void
_outArrayRef(str, node)
	StringInfo str;
	ArrayRef node;
{
	char *s;
	char buf[500];

	sprintf(buf, "arrayref");
	appendStringInfo(str, buf);
	sprintf(buf, " :refelemtype %d", node->refelemtype);
	appendStringInfo(str, buf);
	sprintf(buf, " :refelemlength %d", node->refelemlength);
	appendStringInfo(str, buf);
	sprintf(buf, " :refelembyval %c", (node->refelembyval) ? 't' : 'f');
	appendStringInfo(str, buf);
	sprintf(buf, " :refindex ");
	appendStringInfo(str, buf);
	s = lispOut(node->refindexpr);
	appendStringInfo(str, s);
	sprintf(buf, " :refexpr ");
	appendStringInfo(str, buf);
	s = lispOut(node->refexpr);
	appendStringInfo(str, s);
}

/*
 *  Func is a subclass of Expr
 */

void
_outFunc(str, node)
	StringInfo str;
	Func	node;
{
	char buf[500];
	char *s;

	sprintf(buf, "func");
	appendStringInfo(str,buf);
	sprintf(buf, " :funcid %ld", node->funcid);
	appendStringInfo(str,buf);
	sprintf(buf, " :functype %ld", node->functype);
	appendStringInfo(str,buf);
	sprintf(buf, " :funcisindex %s",
		(node->funcisindex ? "true" : "nil"));
	appendStringInfo(str,buf);
	appendStringInfo(str, " :func_tlist ");
	s = lispOut(node->func_tlist);
	appendStringInfo(str, s);
	pfree(s);
}

/*
 *  Oper is a subclass of Expr
 */

void
_outOper(str, node)
	StringInfo str;
	Oper	node;
{
	char buf[500];

	sprintf(buf, "oper");
	appendStringInfo(str,buf);
	sprintf(buf, " :opno %ld", node->opno);
	appendStringInfo(str,buf);
	sprintf(buf, " :opid %ld", node->opid);
	appendStringInfo(str,buf);
	sprintf(buf, " :oprelationlevel %s",
		(node->oprelationlevel ? "non-nil" : "nil"));
	appendStringInfo(str,buf);
	sprintf(buf, " :opresulttype %ld", node->opresulttype);
	appendStringInfo(str,buf);

}

/*
 *  Param is a subclass of Expr
 */

void
_outParam(str, node)
	StringInfo str;
	Param	node;
{
	char buf[500];
	char *s;

	sprintf(buf, "param");
	appendStringInfo(str,buf);
	sprintf(buf, " :paramkind %d", node->paramkind);
	appendStringInfo(str,buf);
	sprintf(buf, " :paramid %hd", node->paramid);
	appendStringInfo(str,buf);
	sprintf(buf, " :paramname \"%s\"", node->paramname);
	appendStringInfo(str,buf);
	sprintf(buf, " :paramtype %ld", node->paramtype);
	appendStringInfo(str,buf);

	appendStringInfo(str, " :param_tlist ");
	s = lispOut(node->param_tlist);
	appendStringInfo(str, s);
	pfree(s);

}

/*
 *  Stuff from execnodes.h
 */

/*
 *  EState is a subclass of Node.
 */

void
_outEState(str, node)
	StringInfo str;
	EState	node;
{
	char buf[500];

	sprintf(buf, "estate");
	appendStringInfo(str,buf);
	sprintf(buf, " :direction %d", node->es_direction);
	appendStringInfo(str,buf);
	sprintf(buf, " :time %lu", node->es_time);
	appendStringInfo(str,buf);
	sprintf(buf, " :owner %ld", node->es_owner);
	appendStringInfo(str,buf);

	sprintf(buf, " :locks ");
	appendStringInfo(str,buf);
	_outLispValue(str, node->es_locks);

	sprintf(buf, " :subplan_info ");
	appendStringInfo(str,buf);
	_outLispValue(str, node->es_subplan_info);

	sprintf(buf, " :error_message \"%s\"", node->es_error_message);
	appendStringInfo(str,buf);

	sprintf(buf, " :range_table ");
	appendStringInfo(str,buf);
	_outLispValue(str, node->es_range_table);

	/*
	 *  Could be more thorough on the types below, printing more
	 *  info.  Just need to burst out the values in the structs
	 *  pointed to by the entries in the EState node.  For now,
	 *  we'll just print the addresses of the hard stuff.
	 */

	sprintf(buf, " :qualification_tuple @ 0x%x",
			node->es_qualification_tuple);
	appendStringInfo(str,buf);
	sprintf(buf, " :qualification_tuple_id @ 0x%x",
			node->es_qualification_tuple_id);
	appendStringInfo(str,buf);
	sprintf(str, " :relation_relation_descriptor @ 0x%x",
			node->es_relation_relation_descriptor);
	appendStringInfo(str,buf);

	sprintf(str, " :result_relation_info @ 0x%x",
			node->es_result_relation_info);
	appendStringInfo(str,buf);

}

/*
 *  Stuff from relation.h
 */

void
_outRel(str, node)
	StringInfo str;
	Rel	node;
{
	char buf[500];

	sprintf(buf, "rel");
	appendStringInfo(str,buf);

	sprintf(buf, " :relids ");
	appendStringInfo(str,buf);
	_outLispValue(str, node->relids);

	sprintf(buf, " :indexed %s", (node->indexed ? "true" : "nil"));
	appendStringInfo(str,buf);
	sprintf(buf, " :pages %u", node->pages);
	appendStringInfo(str,buf);
	sprintf(buf, " :tuples %u", node->tuples);
	appendStringInfo(str,buf);
	sprintf(buf, " :size %u", node->size);
	appendStringInfo(str,buf);
	sprintf(buf, " :width %u", node->width);
	appendStringInfo(str,buf);

	sprintf(buf, " :targetlist ");
	appendStringInfo(str,buf);
	_outLispValue(str, node->targetlist);

	sprintf(buf, " :pathlist ");
	appendStringInfo(str,buf);
	_outLispValue(str, node->pathlist);

	/*
	 *  Not sure if these are nodes or not.  They're declared as
	 *  struct Path *.  Since i don't know, i'll just print the
	 *  addresses for now.  This can be changed later, if necessary.
	 */

	sprintf(buf, " :unorderedpath @ 0x%x", node->unorderedpath);
	appendStringInfo(str,buf);
	sprintf(buf, " :cheapestpath @ 0x%x", node->cheapestpath);
	appendStringInfo(str,buf);

	sprintf(buf, " :classlist ");
	appendStringInfo(str,buf);
	_outLispValue(str, node->classlist);

	sprintf(buf, " :indexkeys ");
	appendStringInfo(str,buf);
	_outLispValue(str, node->indexkeys);

	sprintf(buf, " :ordering ");
	appendStringInfo(str,buf);
	_outLispValue(str, node->ordering);

	sprintf(buf, " :clauseinfo ");
	appendStringInfo(str,buf);
	_outLispValue(str, node->clauseinfo);

	sprintf(buf, " :joininfo ");
	appendStringInfo(str,buf);
	_outLispValue(str, node->joininfo);

	sprintf(buf, " :innerjoin ");
	appendStringInfo(str,buf);
	_outLispValue(str, node->innerjoin);

}

/*
 *  TLE is a subclass of Node.
 */
/*
void
_outTLE(str, node)
	StringInfo str;
	TLE	node;
{
	char buf[500];

	sprintf(buf, "TLE ");
	appendStringInfo(str,buf);
	sprintf(buf, " :resdom ");
	appendStringInfo(str,buf);
	node->resdom->outFunc(str, node->resdom);

	sprintf(buf, " :expr ");
	appendStringInfo(str,buf);
	node->expr->outFunc(str, node->expr);

}
*/
/*
 *  TL is a subclass of Node.
 */
/*
void
_outTL(str, node)
	StringInfo str;
	TL	node;
{
	char buf[500];

	sprintf(buf, "tl ");
	appendStringInfo(str,buf);
	sprintf(buf, " :entry ");
	appendStringInfo(str,buf);
	node->entry->outFunc(str, node->entry);

	sprintf(buf, " :joinlist ");
	appendStringInfo(str,buf);
	_outLispValue(str, node->joinlist);

}
*/
/*
 *  SortKey is a subclass of Node.
 */

void
_outSortKey(str, node)
	StringInfo str;
	SortKey		node;
{
	char buf[500];

	sprintf(buf, "sortkey");
	appendStringInfo(str,buf);

	sprintf(buf, " :varkeys ");
	appendStringInfo(str,buf);
	_outLispValue(str, node->varkeys);

	sprintf(buf, " :sortkeys ");
	appendStringInfo(str,buf);
	_outLispValue(str, node->sortkeys);

	sprintf(buf, " :relid ");
	appendStringInfo(str,buf);
	_outLispValue(str, node->relid);

	sprintf(buf, " :sortorder ");
	appendStringInfo(str,buf);
	_outLispValue(str, node->sortorder);

}

/*
 *  Path is a subclass of Node.
 */

void
_outPath(str, node)
	StringInfo str;
	Path	node;
{
	char buf[500];

	sprintf(buf, "path");
	appendStringInfo(str,buf);

	sprintf(buf, " :pathtype %ld", node->pathtype);
	appendStringInfo(str,buf);

/*	sprintf(buf, " :parent ");
 *      appendStringInfo(str,buf);
 *	_outLispValue(str, node->parent);
 */
	sprintf(buf, " :cost %f", node->path_cost);
	appendStringInfo(str,buf);

	sprintf(buf, " :p_ordering ");
	appendStringInfo(str,buf);
	_outLispValue(str, node->p_ordering);

	sprintf(buf, " :keys ");
	appendStringInfo(str,buf);
	_outLispValue(str, node->keys);

	sprintf(buf, " :pathsortkey ");
	appendStringInfo(str,buf);
	_outLispValue(str, (LispValue)(node->pathsortkey));

}

/*
 *  IndexPath is a subclass of Path.
 */

void
_outIndexPath(str, node)
	StringInfo str;
	IndexPath	node;
{
	char buf[500];

	sprintf(buf, "indexpath");
	appendStringInfo(str,buf);

	sprintf(buf, " :pathtype %ld", node->pathtype);
	appendStringInfo(str,buf);

/*	sprintf(buf, " :parent ");
	appendStringInfo(str,buf);
	_outLispValue(str, node->parent); */

	sprintf(buf, " :cost %f", node->path_cost);
	appendStringInfo(str,buf);

	sprintf(buf, " :p_ordering ");
	appendStringInfo(str,buf);
	_outLispValue(str, node->p_ordering);

	sprintf(buf, " :keys ");
	appendStringInfo(str,buf);
	_outLispValue(str, node->keys);

	sprintf(buf, " :pathsortkey ");
	appendStringInfo(str,buf);
	_outLispValue(str, (LispValue)(node->pathsortkey));

	sprintf(buf, " :indexid ");
	appendStringInfo(str,buf);
	_outLispValue(str, node->indexid);

	sprintf(buf, " :indexqual ");
	appendStringInfo(str,buf);
	_outLispValue(str, node->indexqual);

}

/*
 *  JoinPath is a subclass of Path
 */

void
_outJoinPath(str, node)
	StringInfo str;
	JoinPath	node;
{
	char buf[500];

	sprintf(buf, "joinpath");
	appendStringInfo(str,buf);

	sprintf(buf, " :pathtype %ld", node->pathtype);
	appendStringInfo(str,buf);

/*	sprintf(buf, " :parent ");
	appendStringInfo(str,buf);
	_outLispValue(str, node->parent); */

	sprintf(buf, " :cost %f", node->path_cost);
	appendStringInfo(str,buf);

	sprintf(buf, " :p_ordering ");
	appendStringInfo(str,buf);
	_outLispValue(str, node->p_ordering);

	sprintf(buf, " :keys ");
	appendStringInfo(str,buf);
	_outLispValue(str, node->keys);

	sprintf(buf, " :pathsortkey ");
	appendStringInfo(str,buf);
	_outLispValue(str, (LispValue)(node->pathsortkey));

	sprintf(buf, " :pathclauseinfo ");
	appendStringInfo(str,buf);
	_outLispValue(str, (LispValue)(node->pathclauseinfo));

	/*
	 *  Not sure if these are nodes; they're declared as "struct path *".
	 *  For now, i'll just print the addresses.
	 */

	sprintf(buf, " :outerjoinpath @ 0x%x", node->outerjoinpath);
	appendStringInfo(str,buf);
	sprintf(buf, " :innerjoinpath @ 0x%x", node->innerjoinpath);
	appendStringInfo(str,buf);

	sprintf(buf, " :outerjoincost %f", node->outerjoincost);
	appendStringInfo(str,buf);

	sprintf(buf, " :joinid ");
	appendStringInfo(str,buf);
	_outLispValue(str, node->joinid);

}

/*
 *  MergePath is a subclass of JoinPath.
 */

void
_outMergePath(str, node)
	StringInfo str;
	MergePath	node;
{
	char buf[500];

	sprintf(buf, "mergepath");
	appendStringInfo(str,buf);

	sprintf(buf, " :pathtype %ld", node->pathtype);
	appendStringInfo(str,buf);

/*	sprintf(buf, " :parent ");
	appendStringInfo(str,buf);
	_outLispValue(str, node->parent);  */

	sprintf(buf, " :cost %f", node->path_cost);
	appendStringInfo(str,buf);

	sprintf(buf, " :p_ordering ");
	appendStringInfo(str,buf);
	_outLispValue(str, node->p_ordering);

	sprintf(buf, " :keys ");
	appendStringInfo(str,buf);
	_outLispValue(str, node->keys);

	sprintf(buf, " :pathsortkey ");
	appendStringInfo(str,buf);
	_outLispValue(str, (LispValue)(node->pathsortkey));

	sprintf(buf, " :pathclauseinfo ");
	appendStringInfo(str,buf);
	_outLispValue(str, (LispValue)(node->pathclauseinfo));

	/*
	 *  Not sure if these are nodes; they're declared as "struct path *".
	 *  For now, i'll just print the addresses.
	 */

	sprintf(buf, " :outerjoinpath @ 0x%x", node->outerjoinpath);
	appendStringInfo(str,buf);
	sprintf(buf, " :innerjoinpath @ 0x%x", node->innerjoinpath);
	appendStringInfo(str,buf);

	sprintf(buf, " :outerjoincost %f", node->outerjoincost);
	appendStringInfo(str,buf);

	sprintf(buf, " :joinid ");
	appendStringInfo(str,buf);
	_outLispValue(str, node->joinid);

	sprintf(buf, " :path_mergeclauses ");
	appendStringInfo(str,buf);
	_outLispValue(str, node->path_mergeclauses);

	sprintf(buf, " :outersortkeys ");
	appendStringInfo(str,buf);
	_outLispValue(str, node->outersortkeys);

	sprintf(buf, " :innersortkeys ");
	appendStringInfo(str,buf);
	_outLispValue(str, node->innersortkeys);

}

/*
 *  HashPath is a subclass of JoinPath.
 */

void
_outHashPath(str, node)
	StringInfo str;
	HashPath	node;
{
	char buf[500];

	sprintf(buf, "hashpath");
	appendStringInfo(str,buf);

	sprintf(buf, " :pathtype %ld", node->pathtype);
	appendStringInfo(str,buf);

/*	sprintf(buf, " :parent ");
	appendStringInfo(str,buf);
	_outLispValue(str, node->parent); */

	sprintf(buf, " :cost %f", node->path_cost);
	appendStringInfo(str,buf);

	sprintf(buf, " :p_ordering ");
	appendStringInfo(str,buf);
	_outLispValue(str, node->p_ordering);

	sprintf(buf, " :keys ");
	appendStringInfo(str,buf);
	_outLispValue(str, node->keys);

	sprintf(buf, " :pathsortkey ");
	appendStringInfo(str,buf);
	_outLispValue(str, (LispValue)(node->pathsortkey));

	sprintf(buf, " :pathclauseinfo ");
	appendStringInfo(str,buf);
	_outLispValue(str, (LispValue)(node->pathclauseinfo));

	/*
	 *  Not sure if these are nodes; they're declared as "struct path *".
	 *  For now, i'll just print the addresses.
	 */

	sprintf(buf, " :outerjoinpath @ 0x%x", node->outerjoinpath);
	appendStringInfo(str,buf);
	sprintf(buf, " :innerjoinpath @ 0x%x", node->innerjoinpath);
	appendStringInfo(str,buf);

	sprintf(buf, " :outerjoincost %f", node->outerjoincost);
	appendStringInfo(str,buf);

	sprintf(buf, " :joinid ");
	appendStringInfo(str,buf);
	_outLispValue(str, node->joinid);

	sprintf(buf, " :path_hashclauses ");
	appendStringInfo(str,buf);
	_outLispValue(str, node->path_hashclauses);

	sprintf(buf, " :outerhashkeys ");
	appendStringInfo(str,buf);
	_outLispValue(str, node->outerhashkeys);

	sprintf(buf, " :innerhashkeys ");
	appendStringInfo(str,buf);
	_outLispValue(str, node->innerhashkeys);

}

/*
 *  OrderKey is a subclass of Node.
 */

void
_outOrderKey(str, node)
	StringInfo str;
	OrderKey	node;
{
	char buf[500];

	sprintf(buf, "orderkey");
	appendStringInfo(str,buf);
	sprintf(buf, " :attribute_number %d", node->attribute_number);
	appendStringInfo(str,buf);
	sprintf(buf, " :array_index %d", node->array_index);
	appendStringInfo(str,buf);

}

/*
 *  JoinKey is a subclass of Node.
 */

void
_outJoinKey(str, node)
	StringInfo str;
	JoinKey		node;
{
	char buf[500];

	sprintf(buf, "joinkey");
	appendStringInfo(str,buf);

	sprintf(buf, " :outer ");
	appendStringInfo(str,buf);
	_outLispValue(str, node->outer);

	sprintf(buf, " :inner ");
	appendStringInfo(str,buf);
	_outLispValue(str, node->inner);

}

/*
 *  MergeOrder is a subclass of Node.
 */

void
_outMergeOrder(str, node)
	StringInfo str;
	MergeOrder	node;
{
	char buf[500];

	sprintf(buf, "mergeorder");
	appendStringInfo(str,buf);

	sprintf(buf, " :join_operator %ld", node->join_operator);
	appendStringInfo(str,buf);
	sprintf(buf, " :left_operator %ld", node->left_operator);
	appendStringInfo(str,buf);
	sprintf(buf, " :right_operator %ld", node->right_operator);
	appendStringInfo(str,buf);
	sprintf(buf, " :left_type %ld", node->left_type);
	appendStringInfo(str,buf);
	sprintf(buf, " :right_type %ld", node->right_type);
	appendStringInfo(str,buf);

}

/*
 *  CInfo is a subclass of Node.
 */

void
_outCInfo(str, node)
	StringInfo str;
	CInfo	node;
{
	char buf[500];

	sprintf(buf, "cinfo");
	appendStringInfo(str,buf);

	sprintf(buf, " :clause ");
	appendStringInfo(str,buf);
	_outLispValue(str, (LispValue)(node->clause));

	sprintf(buf, " :selectivity %f", node->selectivity);
	appendStringInfo(str,buf);
	sprintf(buf, " :notclause %s", (node->notclause ? "true" : "nil"));
	appendStringInfo(str,buf);

	sprintf(buf, " :indexids ");
	appendStringInfo(str,buf);
	_outLispValue(str, node->indexids);

	sprintf(buf, " :mergesortorder ");
	appendStringInfo(str,buf);
	_outLispValue(str, (LispValue)(node->mergesortorder));

	sprintf(buf, " :hashjoinoperator %ld", node->hashjoinoperator);
	appendStringInfo(str,buf);

}

/*
 *  JoinMethod is a subclass of Node.
 */

void
_outJoinMethod(str, node)
	StringInfo str;
	register JoinMethod node;
{
	char buf[500];

	sprintf(buf, "joinmethod");
	appendStringInfo(str,buf);

	sprintf(buf, " :jmkeys ");
	appendStringInfo(str,buf);
	_outLispValue(str, node->jmkeys);

	sprintf(buf, " :clauses ");
	appendStringInfo(str,buf);
	_outLispValue(str, node->clauses);


}

/*
 * HInfo is a subclass of JoinMethod.
 */

void
_outHInfo(str, node)
	StringInfo str;
     register HInfo node;
{
	char buf[500];

	sprintf(buf, "hashinfo");
	appendStringInfo(str,buf);

	sprintf(buf, " :hashop ");
	appendStringInfo(str,buf);
	sprintf(buf, "%d",node->hashop);
	appendStringInfo(str,buf);

	sprintf(buf, " :jmkeys ");
	appendStringInfo(str,buf);
	_outLispValue(str, node->jmkeys);

	sprintf(buf, " :clauses ");
	appendStringInfo(str,buf);
	_outLispValue(str, node->clauses);

}

/*
 *  JInfo is a subclass of Node.
 */

void
_outJInfo(str, node)
	StringInfo str;
	JInfo	node;
{
	char buf[500];

	sprintf(buf, "jinfo");
	appendStringInfo(str,buf);

	sprintf(buf, " :otherrels ");
	appendStringInfo(str,buf);
	_outLispValue(str, node->otherrels);

	sprintf(buf, " :jinfoclauseinfo ");
	appendStringInfo(str,buf);
	_outLispValue(str, node->jinfoclauseinfo);

	sprintf(buf, " :mergesortable %s",
			(node->mergesortable ? "true" : "nil"));
	appendStringInfo(str,buf);
	sprintf(buf, " :hashjoinable %s",
			(node->hashjoinable ? "true" : "nil"));
	appendStringInfo(str,buf);

}

/*
 * Print the value of a Datum given its type.
 */
void
_outDatum(str, value, type) 
StringInfo str;
Datum value;
ObjectId type;
{
    char buf[500];
    Size length, typeLength;
    bool byValue;
    HeapTuple typeTuple;
    int i;
    char *s;

    /*
     * find some information about the type and the "real" length
     * of the datum.
     */
    byValue = get_typbyval(type);
    typeLength = get_typlen(type);
    length = datumGetSize(value, type, byValue, typeLength);

    if (byValue) {
	s = (char *) (&value);
	sprintf(buf, " %d { ", length);
	appendStringInfo(str,buf);
	for (i=0; i<sizeof(Datum); i++) {
	    sprintf(buf, "%d ", (int) (s[i]) );
	appendStringInfo(str,buf);
	}
	sprintf(buf, "} ");
	appendStringInfo(str,buf);
    } else { /* !byValue */
	s = (char *) DatumGetPointer(value);
	if (!PointerIsValid(s)) {
	    sprintf(buf, " 0 { } ");
	appendStringInfo(str,buf);
	} else {
	    /*
	     * length is unsigned - very bad to do < comparison to -1 without
	     * casting it to int first!! -mer 8 Jan 1991
	     */
	    if (((int)length) <= -1) {
		length = VARSIZE(s);
	    }
	    sprintf(buf, " %d { ", length);
	appendStringInfo(str,buf);
	    for (i=0; i<length; i++) {
		sprintf(buf, "%d ", (int) (s[i]) );
	appendStringInfo(str,buf);
	    }
	    sprintf(buf, "} ");
	appendStringInfo(str,buf);
	}
    }

}

void
_outIter(str, node)
	StringInfo str;
	Iter	node;
{
	char *s;

	appendStringInfo(str,"iter");

	appendStringInfo(str," :iterexpr ");

	s = lispOut(node->iterexpr);
	appendStringInfo(str, s);

	pfree(s);
}
