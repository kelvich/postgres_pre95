/*
 *  READFUNCS.C -- Reader functions for Postgres tree nodes.
 *
 *	These routines read in and allocate space for plans.
 *	The main function is at the bottom and figures out what particular
 *  	function to use.
 *
 *  	All these functions assume that lsptok already has its string.
 *
 *	XXX many of these functions have never been tested because
 *	    the nodes they support never appear in plans..  also there
 *	    are some nodes which greg needs to finish (they are missing
 *	    or incomplete).  -cim 5/31/90
 */


#include <stdio.h>
#include <math.h>
#include <strings.h>

#include "tmp/postgres.h"

RcsId("$Header$");

#include "access/heapam.h"
#include "access/htup.h"
#include "utils/fmgr.h"
#include "utils/log.h"

#include "catalog/syscache.h"
#include "catalog/pg_type.h"

#include "tags.h"

#include "nodes/pg_lisp.h"
#include "nodes/nodes.h"
#include "nodes/primnodes.h"
#include "nodes/primnodes.a.h"
#include "nodes/plannodes.h"
#include "nodes/execnodes.h"
#include "nodes/execnodes.a.h"
#include "nodes/relation.h"

extern LispValue lispRead();
extern char *lsptok ARGS((char *string, int *length));
extern Datum readValue();

/* ----------------
 *	node creator declarations
 * ----------------
 */
extern Append 		RMakeAppend();
extern CInfo 		RMakeCInfo();
extern Const 		RMakeConst();
extern EState 		RMakeEState();
extern Existential 	RMakeExistential();
extern Expr 		RMakeExpr();
extern Func 		RMakeFunc();
extern HInfo 		RMakeHInfo();
extern Hash 		RMakeHash();
extern HashJoin 	RMakeHashJoin();
extern HashPath 	RMakeHashPath();
extern IndexPath 	RMakeIndexPath();
extern IndexScan 	RMakeIndexScan();
extern JInfo 		RMakeJInfo();
extern Join 		RMakeJoin();
extern JoinKey 		RMakeJoinKey();
extern JoinMethod 	RMakeJoinMethod();
extern JoinPath 	RMakeJoinPath();
extern JoinRuleInfo	RMakeJoinRuleInfo();
extern MergeJoin 	RMakeMergeJoin();
extern MergeOrder 	RMakeMergeOrder();
extern MergePath 	RMakeMergePath();
extern NestLoop 	RMakeNestLoop();
extern Oper 		RMakeOper();
extern OrderKey 	RMakeOrderKey();
extern Param 		RMakeParam();
extern Path 		RMakePath();
extern Plan 		RMakePlan();
extern Recursive 	RMakeRecursive();
extern Rel 		RMakeRel();
extern Resdom 		RMakeResdom();
extern Result 		RMakeResult();
extern Scan 		RMakeScan();
extern SeqScan 		RMakeSeqScan();
extern Sort 		RMakeSort();
extern SortKey 		RMakeSortKey();
extern Temp 		RMakeTemp();
extern Unique		RMakeUnique();
extern Var 		RMakeVar();

/* ----------------
 *	_getPlan
 * ----------------
 */
void
_getPlan(node)
    Plan node;
{
	char *token;
	int length;

	token = lsptok(NULL, &length);    	/* first token is :cost */
	token = lsptok(NULL, &length);    	/* next is the actual cost */

	node->cost = (Cost) atof(token);

	token = lsptok(NULL, &length);		/* skip the :size */
	token = lsptok(NULL, &length);		/* get the plan_size */
	node->plan_size = (Count) atoi(token);

	token = lsptok(NULL, &length);		/* skip the :width */
	token = lsptok(NULL, &length);		/* get the plan_width */
	node->plan_width = (Count) atoi(token);

	token = lsptok(NULL, &length);    	/* eat the :fragment stuff */
	token = lsptok(NULL, &length);          /* get the fragment */
	node->fragment = (Count) atoi(token);

	token = lsptok(NULL, &length);    	/* eat the :parallel stuff */
	token = lsptok(NULL, &length);          /* get the parallel */
	node->parallel = (Count) atoi(token);

	token = lsptok(NULL, &length);    	/* eat the :state stuff */
	token = lsptok(NULL, &length);    	/* now get the state */ 

	if (!strncmp(token, "nil", 3))
	{
		node->state = (struct EState *) NULL;
	} 
	else /* Disgusting hack until I figure out what to do here */
	{
		node->state = (struct EState *) ! NULL;
	}

	token = lsptok(NULL, &length);    	/* eat :qptargetlist */

	node->qptargetlist = lispRead(true);

	token = lsptok(NULL, &length);    	/* eat :qpqual */

	node->qpqual = lispRead(true);

	token = lsptok(NULL, &length);    	/* eat :lefttree */

	node->lefttree = (struct Plan *) lispRead(true);

	token = lsptok(NULL, &length);    	/* eat :righttree */

	node->righttree = (struct Plan *) lispRead(true);

}

/*
 *  Stuff from plannodes.h
 */

/* ----------------
 *	_readPlan
 * ----------------
 */
Plan
_readPlan()
{
	char *token;
	int length;
	Plan local_node;

	local_node = RMakePlan();
	
	_getPlan(local_node);
	
	return( local_node );
}

/* ----------------
 *	_readResult
 *
 * 	Does some obscene, possibly unportable, magic with
 *	sizes of things.
 * ----------------
 */
Result
_readResult()
{
	Result	local_node;
	char *token;
	int length;

	local_node = RMakeResult();
	
	_getPlan(local_node);

	token = lsptok(NULL, &length);    	/* eat :resrellevelqual */

	local_node->resrellevelqual = lispRead(true);  /* now read it */

	token = lsptok(NULL, &length);    	/* eat :resconstantqual */

	local_node->resconstantqual = lispRead(true);	/* now read it */

	return( local_node );
}

/* ----------------
 *	_readExistential
 *
 *  Existential is a subclass of Plan.
 *  In fact, it is identical.
 * 
 *| Wrong! an Existential inherits Plan and has a field called exstate
 *| -cim 5/31/90
 * ----------------
 */

Existential
_readExistential()
{
	Existential	local_node;

	local_node = RMakeExistential();

	/* ----------------
	 *	XXX this doesn't read the exstate field... -cim
	 * ----------------
	 */
	_getPlan(local_node);
	
	return( local_node );
}

/* ----------------
 *	_readAppend
 *
 *  Append is a subclass of Plan.
 * ----------------
 */

Append
_readAppend()
{
	Append local_node;
	char *token;
	int length;

	local_node = RMakeAppend();

	_getPlan(local_node);

	token = lsptok(NULL, &length);    		/* eat :unionplans */

	local_node->unionplans = lispRead(true); 	/* now read it */
	
	token = lsptok(NULL, &length);    		/* eat :unionrelid */
	token = lsptok(NULL, &length);    		/* get unionrelid */

	local_node->unionrelid = atoi(token);

	token = lsptok(NULL, &length);    	/* eat :unionrtentries */

	local_node->unionrtentries = lispRead(true);	/* now read it */

	return(local_node);
}


/* ----------------
 *	_readRecursive
 *
 *  Recursive is a subclass of Plan.
 * ----------------
 */
Recursive
_readRecursive()
{
        Recursive local_node;
        char *token;
        int length;

        local_node = RMakeRecursive();

        _getPlan(local_node);

        token = lsptok(NULL, &length);               /* eat :recurMethod */
        token = lsptok(NULL, &length);               /* get recurMethod */
        local_node->recurMethod = (RecursiveMethod) atoi(token);

        token = lsptok(NULL, &length);               /* eat :recurCommand */
        local_node->recurCommand = lispRead(true); /* now read it */

        token = lsptok(NULL, &length);               /* eat :recurInitPlans */
        local_node->recurInitPlans = lispRead(true); /* now read it */

        token = lsptok(NULL, &length);               /* eat :recurLoopPlans */
        local_node->recurLoopPlans = lispRead(true); /* now read it */

        token = lsptok(NULL, &length);		/* eat :recurCleanupPlans */
        local_node->recurCleanupPlans = lispRead(true); /* now read it */

        token = lsptok(NULL, &length);       	/* eat :recurCheckpoints */
        local_node->recurCheckpoints = lispRead(true); /* now read it */

        token = lsptok(NULL, &length);       /* eat :recurResultRelationName */
        local_node->recurResultRelationName = lispRead(true); /* now read it */

        return(local_node);
}

/* ----------------
 * 	_readJoinRuleInfo
 *
 * ----------------
 */
JoinRuleInfo
_readJoinRuleInfo()
{
	JoinRuleInfo	local_node;
	char *token;
	int length;

	local_node = RMakeJoinRuleInfo();

	token = lsptok(NULL, &length);    		/* eat :operator */
	token = lsptok(NULL, &length);    		/* get operator */
	
	local_node->jri_operator = atol(token);

	token = lsptok(NULL, &length);    		/* eat :inattrno */
	token = lsptok(NULL, &length);    		/* get inattrno */
	
	local_node->jri_inattrno = atoi(token);

	token = lsptok(NULL, &length);    		/* eat :outattrno */
	token = lsptok(NULL, &length);    		/* get outattrno */
	
	local_node->jri_outattrno = atoi(token);

	/*
	 * a rule lock is printed as a string enclosed in double
	 * quotes. So, we first read the string and then
	 * call `StringToRuleLock()' to get the actual RuleLock
	 * structure.
	 */
	token = lsptok(NULL, &length);    		/* eat :lock */
	token = lsptok(NULL, &length);			/* get lock */
	/*
	 * peel off the double quotes...
	 */
	token++;
	token[length - 2] = '\0';
	local_node->jri_lock = StringToRuleLock(token);
	token[length - 2] = '\"';

	token = lsptok(NULL, &length);    		/* eat :ruleid */
	token = lsptok(NULL, &length);    		/* get ruleId */
	
	local_node->jri_ruleid = atol(token);

	token = lsptok(NULL, &length);    		/* eat :stubid */
	token = lsptok(NULL, &length);    		/* get stubid */
	
	local_node->jri_stubid = atoi(token);

	/*
	 * remember: JoinRuleInfo->stub is only temporarily used.
	 * so its actual value is of no interest.
	 * Always initialize it to NULL
	 */
	token = lsptok(NULL, &length);    		/* eat :stub */
	token = lsptok(NULL, &length);    		/* skip stub */
	local_node->jri_stub = (Prs2OneStub) NULL;

	return(local_node);
}

/* ----------------
 *	_getJoin
 *
 * In case Join is not the same structure as Plan someday.
 * ----------------
 */
void
_getJoin(node)
    Join node;
{
	char		*token;
	int length;

	_getPlan(node);

	token = lsptok(NULL, &length);	/* skip ruleinfo: */
	node->ruleinfo = (struct JoinRuleInfo *) lispRead(true);

}


/* ----------------
 *	_readJoin
 *
 *  Join is a subclass of Plan
 * ----------------
 */
Join
_readJoin()
{
	Join	local_node;

	local_node = RMakeJoin();

	_getJoin(local_node);
	
	return( local_node );
}

/* ----------------
 *	_readNestLoop
 *	
 *  NestLoop is a subclass of Join
 * ----------------
 */

NestLoop
_readNestLoop()
{
	NestLoop	local_node;

	local_node = RMakeNestLoop();

	_getJoin(local_node);
	
	return( local_node );
}

/* ----------------
 *	_readMergeJoin
 *	
 *  MergeJoin is a subclass of Join
 * ----------------
 */

MergeJoin
_readMergeJoin()
{
	MergeJoin	local_node;
	char		*token;
	int length;

	local_node = RMakeMergeJoin();

	_getJoin(local_node);
	token = lsptok(NULL, &length);    		/* eat :mergeclauses */
	
	local_node->mergeclauses = lispRead(true);	/* now read it */

	token = lsptok(NULL, &length);    		/* eat :mergesortop */

	token = lsptok(NULL, &length);    		/* get mergesortop */

	local_node->mergesortop = atol(token);

	return( local_node );
}

/* ----------------
 *	_readHashJoin
 *	
 *  HashJoin is a subclass of Join.
 * ----------------
 */

HashJoin
_readHashJoin()
{
	HashJoin	local_node;
	char 		*token;
	int length;

	local_node = RMakeHashJoin();

	_getJoin(local_node);

	token = lsptok(NULL, &length);    		/* eat :hashclauses */
	
	local_node->hashclauses = lispRead(true);	/* now read it */

	token = lsptok(NULL, &length);    		/* eat :hashjoinop */

	token = lsptok(NULL, &length);    		/* get hashjoinop */

	local_node->hashjoinop = atoi(token);
	
	return( local_node );
}

/* ----------------
 *	_getScan
 *
 *  Scan is a subclass of Node
 *  (Actually, according to the plannodes.h include file, it is a
 *  subclass of Plan.  This is why _getPlan is used here.)
 *
 *  Scan gets its own get function since stuff inherits it.
 * ----------------
 */
void
_getScan(node)
    Scan node;
{
	char *token;
	int length;

	_getPlan(node);
	
	token = lsptok(NULL, &length);    		/* eat :scanrelid */

	token = lsptok(NULL, &length);    		/* get scanrelid */

	node->scanrelid = atoi(token);
}

/* ----------------
 *	_readScan
 *	
 * Scan is a subclass of Plan (Not Node, see above).
 * ----------------
 */
Scan
_readScan()
{
	Scan 	local_node;

	local_node = RMakeScan();

	_getScan(local_node);

	return(local_node);
}

/* ----------------
 *	_readSeqScan
 *	
 *  SeqScan is a subclass of Scan
 * ----------------
 */
SeqScan
_readSeqScan()
{
	SeqScan 	local_node;

	local_node = RMakeSeqScan();

	_getScan(local_node);

	return(local_node);
}

/* ----------------
 *	_readIndexScan
 *	
 *  IndexScan is a subclass of Scan
 * ----------------
 */
IndexScan
_readIndexScan()
{
	IndexScan	local_node;
	char		*token;
	int length;

	local_node = RMakeIndexScan();

	_getScan(local_node);

	token = lsptok(NULL, &length);    		/* eat :indxid */

	local_node->indxid = lispRead(true);		/* now read it */
	
	token = lsptok(NULL, &length);    		/* eat :indxqual */

	local_node->indxqual = lispRead(true); 		/* now read it */

	return(local_node);
}

/* ----------------
 *	_readTemp
 *	
 *  Temp is a subclass of Plan
 * ----------------
 */
Temp
_readTemp()
{
	Temp		local_node;
	char		*token;
	int length;

	local_node = RMakeTemp();

	_getPlan(local_node);

	token = lsptok(NULL, &length);    		/* eat :tempid */
	token = lsptok(NULL, &length);    		/* get tempid */

	local_node->tempid = atol(token);

	token = lsptok(NULL, &length);    		/* eat :keycount */

	token = lsptok(NULL, &length);    		/* get keycount */

	local_node->keycount = atoi(token);

	return(local_node);
}

/* ----------------
 *	_readSort
 *	
 *  Sort is a subclass of Temp
 * ----------------
 */
Sort
_readSort()
{
	Sort		local_node;
	char		*token;
	int length;

	local_node = RMakeSort();

	_getPlan(local_node);

	token = lsptok(NULL, &length);    		/* eat :tempid */
	token = lsptok(NULL, &length);    		/* get tempid */

	local_node->tempid = atol(token);

	token = lsptok(NULL, &length);    		/* eat :keycount */

	token = lsptok(NULL, &length);    		/* get keycount */

	local_node->keycount = atoi(token);

	return(local_node);
}

/* ----------------
 *	_readUnique
 *
 * For some reason, unique is a subclass of Temp.
 */
Unique
_readUnique()
{
	Unique		local_node;
	char		*token;
	int		length;

	local_node = RMakeUnique();

	_getPlan(local_node);

	token = lsptok(NULL, &length);    		/* eat :tempid */
	token = lsptok(NULL, &length);    		/* get :tempid */
	local_node->tempid = atol(token);

	token = lsptok(NULL, &length);    		/* eat :keycount */
	token = lsptok(NULL, &length);    		/* get :keycount */
	local_node->keycount = atoi(token);

	return(local_node);
}

/* ----------------
 *	_readHash
 *	
 *  Hash is a subclass of Temp
 * ----------------
 */
Hash
_readHash()
{
	Hash		local_node;
	char		*token;
	int length;

	local_node = RMakeHash();

	_getPlan(local_node);

	token = lsptok(NULL, &length);    		/* eat :hashkey */
	local_node->hashkey = (Var) lispRead(true);

	return(local_node);
}

/*
 *  Stuff from primnodes.h.
 */

/* ----------------
 *	_readResdom
 *	
 *  Resdom is a subclass of Node
 * ----------------
 */
Resdom
_readResdom()
{
	Resdom		local_node;
	char		*token;
	int length;

	local_node = RMakeResdom();

	token = lsptok(NULL, &length);    		/* eat :resno */
	token = lsptok(NULL, &length);    		/* get resno */

	local_node->resno = atoi(token);

	token = lsptok(NULL, &length);    		/* eat :restype */
	token = lsptok(NULL, &length);    		/* get restype */

	local_node->restype = atol(token);

	token = lsptok(NULL, &length);    		/* eat :reslen */
	token = lsptok(NULL, &length);    		/* get reslen */

	local_node->reslen = atoi(token);

	token = lsptok(NULL, &length);    		/* eat :resname */
	token = lsptok(NULL, &length);    		/* get the name */

	if (!strncmp(token, "\"null\", 5"))
	{
		local_node->resname = NULL;
	}
	else
	{
		/*
		 * Peel off ""'s, then make a true copy.
		 */

		token++;
		token[length - 2] = '\0';

		local_node->resname = (Name) palloc(sizeof(char16));
		strcpy(local_node->resname, token);
		token[length - 2] = '\"';
	}

	token = lsptok(NULL, &length);    		/* eat :reskey */
	token = lsptok(NULL, &length);    		/* get reskey */

	local_node->reskey = atoi(token);

	token = lsptok(NULL, &length);    		/* eat :reskeyop */

	token = lsptok(NULL, &length);    		/* get reskeyop */

	local_node->reskeyop = (OperatorTupleForm) atol(token);

	token = lsptok(NULL, &length);    		/* eat :resjunk */
	token = lsptok(NULL, &length);    		/* get resjunk */
	local_node->resjunk = atoi(token);

	return(local_node);
}

/* ----------------
 *	_readExpr
 *	
 *  Expr is a subclass of Node
 * ----------------
 */
Expr
_readExpr()
{
	Expr local_node;

	local_node = RMakeExpr();

	return(local_node);
}

/* ----------------
 *	_readVar
 *	
 *  Var is a subclass of Expr
 * ----------------
 */
Var
_readVar()
{
	Var		local_node;
	char		*token;
	int length;

	local_node = RMakeVar();

	token = lsptok(NULL, &length);    		/* eat :varno */
	token = lsptok(NULL, &length);    		/* get varno */
	
	local_node->varno = atoi(token);

	token = lsptok(NULL, &length);    		/* eat :varattno */
	token = lsptok(NULL, &length);    		/* get varattno */
	
	local_node->varattno = atoi(token);

	token = lsptok(NULL, &length);    		/* eat :vartype */
	token = lsptok(NULL, &length);    		/* get vartype */
	
	local_node->vartype = (ObjectId) atol(token);

	token = lsptok(NULL, &length);    		/* eat :vardotfields */


	/* What to do about the extra ")"'s??? */

	local_node->vardotfields = lispRead(true); 	/* now read it */

	token = lsptok(NULL, &length);    	       /* eat :vararrayindex */
	token = lsptok(NULL, &length);    		/* get vararrayindex */
	
	local_node->vararrayindex = atoi(token);

	token = lsptok(NULL, &length);    		/* eat :varid */

	local_node->varid = lispRead(true); 		/* now read it */

	token = lsptok(NULL, &length);    		/* eat :varelemtype */
	token = lsptok(NULL, &length);
	local_node->varelemtype = (ObjectId) atoi(token);

	return(local_node);
}

/* ----------------
 *	_readConst
 *	
 *  Const is a subclass of Expr
 * ----------------
 */
Const
_readConst()
{
	Const	local_node;
	char *token;
	int length;

	local_node = RMakeConst();

	token = lsptok(NULL, &length);      /* get :consttype */
	token = lsptok(NULL, &length);      /* now read it */

	local_node->consttype = atol(token);


	token = lsptok(NULL, &length);      /* get :constlen */
	token = lsptok(NULL, &length);      /* now read it */

	local_node->constlen = atoi(token);

	token = lsptok(NULL, &length);      /* get :constisnull */
	token = lsptok(NULL, &length);      /* now read it */

	if (!strncmp(token, "true", 4))
	{
		local_node->constisnull = true;
	}
	else
	{
		local_node->constisnull = false;
	}


	token = lsptok(NULL, &length);      /* get :constvalue */

	if (local_node->constisnull) {
	    token = lsptok(NULL, &length);      /* skip "NIL" */
	} else {
	    /*
	     * read the value
	     */
	    local_node->constvalue = readValue(local_node->consttype);
	}

	token = lsptok(NULL, &length);      /* get :constbyval */
	token = lsptok(NULL, &length);      /* now read it */

	if (!strncmp(token, "true", 4))
	{
		local_node->constbyval = true;
	}
	else
	{
		local_node->constbyval = false;
	}
	
	return(local_node);
}

/* ----------------
 *	_readFunc
 *	
 *  Func is a subclass of Expr
 * ----------------
 */
Func
_readFunc()
{
	Func	local_node;
	char *token;
	int length;

	local_node = RMakeFunc();

	token = lsptok(NULL, &length);      /* get :funcid */
	token = lsptok(NULL, &length);      /* now read it */

	local_node->funcid = atol(token);

	token = lsptok(NULL, &length);      /* get :functype */
	token = lsptok(NULL, &length);      /* now read it */

	local_node->functype = atol(token);

	token = lsptok(NULL, &length);      /* get :funcisindex */
	token = lsptok(NULL, &length);      /* now read it */

	if (!strncmp(token, "true", 4))
	{
		local_node->funcisindex = true;
	}
	else
	{
		local_node->funcisindex = false;
	}
	local_node->func_fcache = (FunctionCache *) NULL;
	
	return(local_node);
}

/* ----------------
 *	_readOper
 *	
 *  Oper is a subclass of Expr
 * ----------------
 */
Oper
_readOper()
{
	Oper	local_node;
	char 	*token;
	int length;

	local_node = RMakeOper();

	token = lsptok(NULL, &length);      /* get :opno */
	token = lsptok(NULL, &length);      /* now read it */

	local_node->opno = atol(token);

	token = lsptok(NULL, &length);      /* get :opid */
	token = lsptok(NULL, &length);      /* now read it */
	local_node->opid = atol(token);

	token = lsptok(NULL, &length);      /* get :oprelationlevel */
	token = lsptok(NULL, &length);      /* now read it */

	if (!strncmp(token, "non-nil", 7))
	{
		local_node->oprelationlevel = true;
	}
	else
	{
		local_node->oprelationlevel = false;
	}

	token = lsptok(NULL, &length);      /* get :opresulttype */
	token = lsptok(NULL, &length);      /* now read it */

	local_node->opresulttype = atol(token);

	/*
	 * NOTE: Alternatively we can call 'replace_opid' 
	 * which initializes both 'opid' and 'op_fcache'.
	 */
	local_node->op_fcache = (FunctionCache *) NULL;

	return(local_node);
}

/* ----------------
 *	_readParam
 *	
 *  Param is a subclass of Expr
 * ----------------
 */
Param
_readParam()
{
	Param	local_node;
	char 	*token;
	int length;

	local_node = RMakeParam();

	token = lsptok(NULL, &length);      /* get :paramkind */
	token = lsptok(NULL, &length);      /* now read it */

	local_node->paramkind = atoi(token);

	token = lsptok(NULL, &length);      /* get :paramid */
	token = lsptok(NULL, &length);      /* now read it */

	local_node->paramid = atol(token);

	token = lsptok(NULL, &length);      /* get :paramname */
	token = lsptok(NULL, &length);      /* now read it */
	token++;			    /* skip the first `"' */
	token[length - 2] = '\0';	    /* this is the 2nd `"' */

	local_node->paramname = (Name) palloc(sizeof(char16));
	strcpy(local_node->paramname, token);
	token[length - 2] = '\"';	/* restore the 2nd `"' */

	token = lsptok(NULL, &length);      /* get :paramtype */
	token = lsptok(NULL, &length);      /* now read it */

	local_node->paramtype = atol(token);
	
	return(local_node);
}

/*
 *  Stuff from execnodes.h
 */

/* ----------------
 *	_readEState
 *	
 *  EState is a subclass of Node.
 * ----------------
 */
EState
_readEState()
{
	EState	local_node;
	char *token;
	int length;

	local_node = RMakeEState();

	token = lsptok(NULL, &length);      /* get :direction */
	token = lsptok(NULL, &length);      /* now read it */

	local_node->es_direction = atoi(token);

	token = lsptok(NULL, &length);      /* get :time */
	token = lsptok(NULL, &length);      /* now read it */

	local_node->es_time = (unsigned long) atoi(token);

	token = lsptok(NULL, &length);      /* get :owner */
	token = lsptok(NULL, &length);      /* now read it */

	local_node->es_owner = atol(token);

	token = lsptok(NULL, &length);      /* get :locks */

	local_node->es_locks = lispRead(true);   /* now read it */

	token = lsptok(NULL, &length);      /* get :subplan_info */

	local_node->es_subplan_info = lispRead(true); /* now read it */

	token = lsptok(NULL, &length);      /* get :error_message */
	token = lsptok(NULL, &length);      /* now read it */
	token++;
	token[length-2] = '\0';

	local_node->es_error_message = (Name) palloc(sizeof(char16));
	strcpy(local_node->es_error_message, token);

	token[length-2] = '\"';

	token = lsptok(NULL, &length);      /* get :range_table */

	local_node->es_locks = lispRead(true); /* now read it */

	token = lsptok(NULL, &length);      /* get :qualification_tuple */
	token = lsptok(NULL, &length);      /* get @ */
	token = lsptok(NULL, &length);      /* now read it */

	sscanf(token, "%x", &local_node->es_qualification_tuple);

	token = lsptok(NULL, &length);      /* get :qualification_tuple_id */
	token = lsptok(NULL, &length);      /* get @ */
	token = lsptok(NULL, &length);      /* now read it */

	sscanf(token, "%x", &local_node->es_qualification_tuple_id);

	token = lsptok(NULL, &length); /* get :relation_relation_descriptor */
	token = lsptok(NULL, &length);      /* get @ */
	token = lsptok(NULL, &length);      /* now read it */

	sscanf(token, "%x", &local_node->es_relation_relation_descriptor);

	token = lsptok(NULL, &length);      /* get :result_relation_info */
	token = lsptok(NULL, &length);      /* get @ */
	token = lsptok(NULL, &length);      /* now read it */

	sscanf(token, "%x", &local_node->es_result_relation_info);
	
	return(local_node);
}

/*
 *  Stuff from relation.h
 */

/* ----------------
 *	_readRel
 * ----------------
 */
Rel
_readRel()
{
	Rel	local_node;
	char 	*token;
	int length;

	local_node = RMakeRel();

	token = lsptok(NULL, &length);      /* get :relids */
	local_node->relids = lispRead(true);/* now read it */

	token = lsptok(NULL, &length);      /* get :indexed */
	token = lsptok(NULL, &length);      /* now read it */

	if (!strncmp(token, "true", 4))
	{
		local_node->indexed = true;
	}
	else
	{
		local_node->indexed = false;
	}

	token = lsptok(NULL, &length);      /* get :pages */
	token = lsptok(NULL, &length);      /* now read it */

	local_node->pages = (unsigned int) atoi(token);

	token = lsptok(NULL, &length);      /* get :tuples */
	token = lsptok(NULL, &length);      /* now read it */

	local_node->tuples = (unsigned int) atoi(token);

	token = lsptok(NULL, &length);      /* get :size */
	token = lsptok(NULL, &length);      /* now read it */

	local_node->size = (unsigned int) atoi(token);

	token = lsptok(NULL, &length);      /* get :width */
	token = lsptok(NULL, &length);      /* now read it */

	local_node->width = (unsigned int) atoi(token);

	token = lsptok(NULL, &length);      /* get :targetlist */
	local_node->targetlist = lispRead(true); /* now read it */

	token = lsptok(NULL, &length);      /* get :pathlist */
	local_node->pathlist = lispRead(true); /* now read it */

	/*
	 *  Not sure if these are nodes or not.  They're declared as
	 *  struct Path *.  Since i don't know, i'll just print the
	 *  addresses for now.  This can be changed later, if necessary.
	 */

	token = lsptok(NULL, &length);      /* get :unorderpath */
	token = lsptok(NULL, &length);      /* get @ */
	token = lsptok(NULL, &length);      /* now read it */

	sscanf(token, "%x", &local_node->unorderedpath);

	token = lsptok(NULL, &length);      /* get :cheapestpath */
	token = lsptok(NULL, &length);      /* get @ */
	token = lsptok(NULL, &length);      /* now read it */

	sscanf(token, "%x", &local_node->cheapestpath);

	token = lsptok(NULL, &length);      /* get :classlist */
	local_node->classlist = lispRead(true); /* now read it */

	token = lsptok(NULL, &length);      /* get :indexkeys */
	local_node->indexkeys = lispRead(true);/* now read it */

	token = lsptok(NULL, &length);      /* get :ordering */
	local_node->ordering = lispRead(true); /* now read it */

	token = lsptok(NULL, &length);      /* get :clauseinfo */
	local_node->clauseinfo = lispRead(true); /* now read it */

	token = lsptok(NULL, &length);      /* get :joininfo */
	local_node->joininfo = lispRead(true); /* now read it */

	token = lsptok(NULL, &length);      /* get :innerjoin */
	local_node->innerjoin = lispRead(true); /* now read it */
	
	return(local_node);
}

/* ----------------
 *	_readSortKey
 *	
 *  SortKey is a subclass of Node.
 * ----------------
 */

SortKey
_readSortKey()
{
	SortKey		local_node;
	char		*token;
	int length;

	local_node = RMakeSortKey();
	token = lsptok(NULL, &length);      /* get :varkeys */
	local_node->varkeys = lispRead(true); /* now read it */

	token = lsptok(NULL, &length);      /* get :sortkeys */
	local_node->sortkeys = lispRead(true); /* now read it */

	token = lsptok(NULL, &length);      /* get :relid */
	local_node->relid = lispRead(true);      /* now read it */

	token = lsptok(NULL, &length);      /* get :sortorder */
	local_node->sortorder = lispRead(true);      /* now read it */
	
	return(local_node);
}

/* ----------------
 *	_readPath
 *	
 *  Path is a subclass of Node.
 * ----------------
 */
Path
_readPath()
{
	Path	local_node;
	char 	*token;
	int length;

	local_node = RMakePath();

	token = lsptok(NULL, &length);      /* get :pathtype */
	token = lsptok(NULL, &length);      /* now read it */

	local_node->pathtype = atol(token);

	token = lsptok(NULL, &length);      /* get :cost */
	token = lsptok(NULL, &length);      /* now read it */

	local_node->path_cost = (Cost) atof(token);

	token = lsptok(NULL, &length);      /* get :p_ordering */
	local_node->p_ordering = lispRead(true);      /* now read it */

	token = lsptok(NULL, &length);      /* get :keys */
	local_node->keys = lispRead(true);       /* now read it */

	token = lsptok(NULL, &length);      /* get :sortpath */
	local_node->sortpath = (SortKey) lispRead(true);

	return(local_node);
}

/* ----------------
 *	_readIndexPath
 *	
 *  IndexPath is a subclass of Path.
 * ----------------
 */
IndexPath
_readIndexPath()
{
	IndexPath	local_node;
	char		*token;
	int length;

	local_node = RMakeIndexPath();

	token = lsptok(NULL, &length);      /* get :pathtype */
	token = lsptok(NULL, &length);      /* now read it */

	local_node->pathtype = atol(token);

	token = lsptok(NULL, &length);      /* get :cost */
	token = lsptok(NULL, &length);      /* now read it */

	local_node->path_cost = (Cost) atof(token);

	token = lsptok(NULL, &length);      /* get :p_ordering */
	local_node->p_ordering = lispRead(true);      /* now read it */

	token = lsptok(NULL, &length);      /* get :keys */
	local_node->keys = lispRead(true);       /* now read it */

	token = lsptok(NULL, &length);      /* get :sortpath */
	local_node->sortpath = (SortKey) lispRead(true);

	token = lsptok(NULL, &length);      /* get :indexid */
	local_node->indexid = lispRead(true);

	token = lsptok(NULL, &length);      /* get :indexqual */
	local_node->indexqual = lispRead(true);      /* now read it */

	return(local_node);
}

/* ----------------
 *	_readJoinPath
 *	
 *  JoinPath is a subclass of Path
 * ----------------
 */
JoinPath
_readJoinPath()
{
	JoinPath	local_node;
	char		*token;
	int length;


	local_node = RMakeJoinPath();

	token = lsptok(NULL, &length);      /* get :pathtype */
	token = lsptok(NULL, &length);      /* now read it */

	local_node->pathtype = atol(token);

	token = lsptok(NULL, &length);      /* get :cost */
	token = lsptok(NULL, &length);      /* now read it */

	local_node->path_cost = (Cost) atof(token);

	token = lsptok(NULL, &length);      /* get :p_ordering */
	local_node->p_ordering = lispRead(true);           /* now read it */

	token = lsptok(NULL, &length);      /* get :keys */
	local_node->keys = lispRead(true);            /* now read it */

	token = lsptok(NULL, &length);      /* get :sortpath */
	local_node->sortpath = (SortKey) lispRead(true);

	token = lsptok(NULL, &length);      /* get :pathclauseinfo */
	local_node->pathclauseinfo = lispRead(true);         /* now read it */

	/*
	 *  Not sure if these are nodes; they're declared as "struct path *".
	 *  For now, i'll just print the addresses.
	 *
	 * GJK:  Since I am parsing this stuff, I'll just ignore the addresses,
	 * and initialize these pointers to NULL.
	 */

	token = lsptok(NULL, &length);      /* get :outerjoinpath */
	token = lsptok(NULL, &length);      /* get @ */
	token = lsptok(NULL, &length);      /* now read it */

	local_node->outerjoinpath = NULL;

	token = lsptok(NULL, &length);      /* get :innerjoinpath */
	token = lsptok(NULL, &length);      /* get @ */
	token = lsptok(NULL, &length);      /* now read it */

	local_node->innerjoinpath = NULL;

	token = lsptok(NULL, &length);      /* get :outerjoincost */
	token = lsptok(NULL, &length);      /* now read it */

	local_node->outerjoincost = (Cost) atof(token);

	token = lsptok(NULL, &length);      /* get :joinid */
	local_node->joinid = lispRead(true);          /* now read it */

	return(local_node);
}

/* ----------------
 *	_readMergePath
 *	
 *  MergePath is a subclass of JoinPath.
 * ----------------
 */

MergePath
_readMergePath()
{
	MergePath	local_node;
	char 		*token;
	int length;

	local_node = RMakeMergePath();

	token = lsptok(NULL, &length);      /* get :pathtype */
	token = lsptok(NULL, &length);      /* now read it */

	local_node->pathtype = atol(token);

	token = lsptok(NULL, &length);      /* get :cost */
	token = lsptok(NULL, &length);      /* now read it */

	local_node->path_cost = (Cost) atof(token);

	token = lsptok(NULL, &length);      /* get :p_ordering */
	local_node->p_ordering = lispRead(true);           /* now read it */

	token = lsptok(NULL, &length);      /* get :keys */
	local_node->keys = lispRead(true);            /* now read it */

	token = lsptok(NULL, &length);      /* get :sortpath */
	local_node->sortpath = (SortKey) lispRead(true);

	token = lsptok(NULL, &length);      /* get :pathclauseinfo */
	local_node->pathclauseinfo = lispRead(true);        /* now read it */

	/*
	 *  Not sure if these are nodes; they're declared as "struct path *".
	 *  For now, i'll just print the addresses.
	 *
	 * GJK:  Since I am parsing this stuff, I'll just ignore the addresses,
	 * and initialize these pointers to NULL.
	 */

	token = lsptok(NULL, &length);      /* get :outerjoinpath */
	token = lsptok(NULL, &length);      /* get @ */
	token = lsptok(NULL, &length);      /* now read it */

	local_node->outerjoinpath = NULL;

	token = lsptok(NULL, &length);      /* get :innerjoinpath */
	token = lsptok(NULL, &length);      /* get @ */
	token = lsptok(NULL, &length);      /* now read it */

	local_node->innerjoinpath = NULL;

	token = lsptok(NULL, &length);      /* get :outerjoincost */
	token = lsptok(NULL, &length);      /* now read it */

	local_node->outerjoincost = (Cost) atof(token);

	token = lsptok(NULL, &length);      /* get :joinid */
	local_node->joinid = lispRead(true);          /* now read it */

	token = lsptok(NULL, &length);      /* get :path_mergeclauses */
	local_node->path_mergeclauses = lispRead(true);      /* now read it */

	token = lsptok(NULL, &length);      /* get :outersortkeys */
	local_node->outersortkeys = lispRead(true);           /* now read it */

	token = lsptok(NULL, &length);      /* get :innersortkeys */
	local_node->innersortkeys = lispRead(true);           /* now read it */

	return(local_node);
}

/* ----------------
 *	_readHashPath
 *	
 *  HashPath is a subclass of JoinPath.
 * ----------------
 */
HashPath
_readHashPath()
{
	HashPath	local_node;
	char 		*token;
	int length;

	local_node = RMakeHashPath();

	token = lsptok(NULL, &length);      /* get :pathtype */
	token = lsptok(NULL, &length);      /* now read it */

	local_node->pathtype = atol(token);

	token = lsptok(NULL, &length);      /* get :cost */
	token = lsptok(NULL, &length);      /* now read it */

	local_node->path_cost = (Cost) atof(token);

	token = lsptok(NULL, &length);      /* get :p_ordering */
	local_node->p_ordering = lispRead(true); /* now read it */

	token = lsptok(NULL, &length);      /* get :keys */
	local_node->keys = lispRead(true);       /* now read it */

	token = lsptok(NULL, &length);      /* get :sortpath */
	local_node->sortpath = (SortKey) lispRead(true);

	token = lsptok(NULL, &length);      /* get :pathclauseinfo */
	local_node->pathclauseinfo = lispRead(true); /* now read it */

	/*
	 *  Not sure if these are nodes; they're declared as "struct path *".
	 *  For now, i'll just print the addresses.
	 *
	 * GJK:  Since I am parsing this stuff, I'll just ignore the addresses,
	 * and initialize these pointers to NULL.
	 */

	token = lsptok(NULL, &length);      /* get :outerjoinpath */
	token = lsptok(NULL, &length);      /* get @ */
	token = lsptok(NULL, &length);      /* now read it */

	local_node->outerjoinpath = NULL;

	token = lsptok(NULL, &length);      /* get :innerjoinpath */
	token = lsptok(NULL, &length);      /* get @ */
	token = lsptok(NULL, &length);      /* now read it */

	local_node->innerjoinpath = NULL;

	token = lsptok(NULL, &length);      /* get :outerjoincost */
	token = lsptok(NULL, &length);      /* now read it */

	local_node->outerjoincost = (Cost) atof(token);

	token = lsptok(NULL, &length);      /* get :joinid */
	local_node->joinid = lispRead(true);     /* now read it */

	token = lsptok(NULL, &length);      /* get :path_hashclauses */
	local_node->path_hashclauses = lispRead(true); /* now read it */

	token = lsptok(NULL, &length);      /* get :outerhashkeys */
	local_node->outerhashkeys = lispRead(true); /* now read it */

	token = lsptok(NULL, &length);      /* get :innerhashkeys */
	local_node->innerhashkeys = lispRead(true); /* now read it */

	return(local_node);
}

/* ----------------
 *	_readOrderKey
 *	
 *  OrderKey is a subclass of Node.
 * ----------------
 */
OrderKey
_readOrderKey()
{
	OrderKey	local_node;
	char		*token;
	int length;

	local_node = RMakeOrderKey();

	token = lsptok(NULL, &length);      /* get :attribute_number */
	token = lsptok(NULL, &length);      /* now read it */

	local_node->attribute_number = atoi(token);
	
	token = lsptok(NULL, &length);      /* get :array_index */
	token = lsptok(NULL, &length);      /* now read it */

	local_node->array_index = atoi(token);

	return(local_node);
}

/* ----------------
 *	_readJoinKey
 *	
 *  JoinKey is a subclass of Node.
 * ----------------
 */

JoinKey
_readJoinKey()
{
	JoinKey		local_node;
	char		*token;
	int length;

	local_node = RMakeJoinKey();

	token = lsptok(NULL, &length);      /* get :outer */
	local_node->outer = lispRead(true);      /* now read it */

	token = lsptok(NULL, &length);      /* get :inner */
	local_node->inner = lispRead(true);      /* now read it */
	
	return(local_node);
}

/* ----------------
 *	_readMergeOrder
 *	
 *  MergeOrder is a subclass of Node.
 * ----------------
 */
MergeOrder
_readMergeOrder()
{
	MergeOrder	local_node;
	char		*token;
	int length;

	local_node = RMakeMergeOrder();
	token = lsptok(NULL, &length);      /* get :join_operator */
	token = lsptok(NULL, &length);      /* now read it */

	local_node->join_operator = atol(token);

	token = lsptok(NULL, &length);      /* get :left_operator */
	token = lsptok(NULL, &length);      /* now read it */

	local_node->left_operator = atol(token);

	token = lsptok(NULL, &length);      /* get :right_operator */
	token = lsptok(NULL, &length);      /* now read it */

	local_node->right_operator = atol(token);

	token = lsptok(NULL, &length);      /* get :left_type */
	token = lsptok(NULL, &length);      /* now read it */

	local_node->left_type = atol(token);

	token = lsptok(NULL, &length);      /* get :right_type */
	token = lsptok(NULL, &length);      /* now read it */

	local_node->right_type = atol(token);

	return(local_node);
}

/* ----------------
 *	_readCInfo
 *	
 *  CInfo is a subclass of Node.
 * ----------------
 */
CInfo
_readCInfo()
{
	CInfo	local_node;
	char 	*token;
	int length;

	local_node = RMakeCInfo();

	token = lsptok(NULL, &length);      /* get :clause */
	local_node->clause = (Expr) lispRead(true); /* now read it */

	token = lsptok(NULL, &length);      /* get :selectivity */
	token = lsptok(NULL, &length);      /* now read it */

	local_node->selectivity = atof(token);

	token = lsptok(NULL, &length);      /* get :notclause */
	token = lsptok(NULL, &length);      /* now read it */

	if (!strncmp(token, "true", 4))
	{
		local_node->notclause = true;
	}
	else
	{
		local_node->notclause = false;
	}
 
	token = lsptok(NULL, &length);      /* get :indexids */
	local_node->indexids = lispRead(true);   /* now read it */

	token = lsptok(NULL, &length);      /* get :mergesortorder */
	local_node->mergesortorder = (MergeOrder) lispRead(true);

	token = lsptok(NULL, &length);      /* get :hashjoinoperator */
	token = lsptok(NULL, &length);      /* now read it */

	local_node->hashjoinoperator = atol(token);

	return(local_node);
}

/* ----------------
 *	_readJoinMethod
 *	
 *  JoinMethod is a subclass of Node.
 * ----------------
 */
JoinMethod
_readJoinMethod()
{
     	JoinMethod 	local_node;
	char		*token;
	int length;

	local_node = RMakeJoinMethod();

	token = lsptok(NULL, &length);      /* get :jmkeys */
	local_node->jmkeys = lispRead(true);/* now read it */

	token = lsptok(NULL, &length);      /* get :clauses */
	local_node->clauses = lispRead(true); /* now read it */

	return(local_node);
}

/* ----------------
 *	_readHInfo
 *	
 * HInfo is a subclass of JoinMethod.
 * ----------------
 */
HInfo
_readHInfo()
{
	HInfo 	local_node;
	char 	*token;
	int length;

	local_node = RMakeHInfo();

	token = lsptok(NULL, &length);      /* get :hashop */
	token = lsptok(NULL, &length);      /* now read it */
	
	local_node->hashop = atoi(token);

	token = lsptok(NULL, &length);      /* get :jmkeys */
	local_node->jmkeys = lispRead(true); /* now read it */

	token = lsptok(NULL, &length);      /* get :clauses */
	local_node->clauses = lispRead(true);           /* now read it */

	return(local_node);
}

/* ----------------
 *	_readJInfo()
 *	
 *  JInfo is a subclass of Node.
 * ----------------
 */
JInfo
_readJInfo()
{
	JInfo	local_node;
	char	*token;
	int length;

	local_node = RMakeJInfo();

	token = lsptok(NULL, &length);      /* get :otherrels */
	local_node->otherrels = lispRead(true); /* now read it */

	token = lsptok(NULL, &length);      /* get :jinfoclauseinfo */
	local_node->jinfoclauseinfo = lispRead(true); /* now read it */

	token = lsptok(NULL, &length);      /* get :mergesortable */

	if (!strncmp(token, "true", 4))
	{
		local_node->mergesortable = true;
	}
	else
	{
		local_node->mergesortable = false;
	}

	token = lsptok(NULL, &length);      /* get :hashjoinable */

	if (!strncmp(token, "true", 4))
	{
		local_node->hashjoinable = true;
	}
	else
	{
		local_node->hashjoinable = false;
	}

	return(local_node);
}


/* ----------------
 *	parsePlanString
 *
 * Given a character string containing a plan, parsePlanString sets up the
 * plan structure representing that plan.
 *
 * The string passed to parsePlanString must be null-terminated.
 * ----------------
 */
LispValue
parsePlanString()
{
	char *token;
	int length;
	LispValue return_value;

	token = lsptok(NULL, &length);

	if (!strncmp(token, "plan", 4))
	{
		return_value = (LispValue) _readPlan();
		return_value->type = T_Plan;
	}
	else if (!strncmp(token, "result", 6))
	{
		return_value = (LispValue) _readResult();
		return_value->type = T_Result;
	}
	else if (!strncmp(token, "existential", 11))
	{
		return_value = (LispValue) _readExistential();
		return_value->type = T_Existential;
	}
	else if (!strncmp(token, "append", 6))
	{
		return_value = (LispValue) _readAppend();
		return_value->type = T_Append;
	}
	else if (!strncmp(token, "joinruleinfo", 12))
	{
		/*
		 * NOTE: 'joinruleinfo' must be checked BEFORE
		 * 'join', because both `strncmp' test will
		 * succeeed !
		 */
		return_value = (LispValue) _readJoinRuleInfo();
		return_value->type = T_JoinRuleInfo;
	}
	else if (!strncmp(token, "join", 4))
	{
		return_value = (LispValue) _readJoin();
		return_value->type = T_Join;
	}
	else if (!strncmp(token, "nestloop", 8))
	{
		return_value = (LispValue) _readNestLoop();
		return_value->type = T_NestLoop;
	}
	else if (!strncmp(token, "mergejoin", 9))
	{
		return_value = (LispValue) _readMergeJoin();
		return_value->type = T_MergeJoin;
	}
	else if (!strncmp(token, "hashjoin", 8))
	{
		return_value = (LispValue) _readHashJoin();
		return_value->type = T_HashJoin;
	}
	else if (!strncmp(token, "scan", 4))
	{
		return_value = (LispValue) _readScan();
		return_value->type = T_Scan;
	}
	else if (!strncmp(token, "seqscan", 7))
	{
		return_value = (LispValue) _readSeqScan();
		return_value->type = T_SeqScan;
	}
	else if (!strncmp(token, "indexscan", 9))
	{
		return_value = (LispValue) _readIndexScan();
		return_value->type = T_IndexScan;
	}
	else if (!strncmp(token, "temp", 4))
	{
		return_value = (LispValue) _readTemp();
		return_value->type = T_Temp;
	}
	else if (!strncmp(token, "sort", 4))
	{
		return_value = (LispValue) _readSort();
		return_value->type = T_Sort;
	}
	else if (!strncmp(token, "hash", 4))
	{
		return_value = (LispValue) _readHash();
		return_value->type = T_Hash;
	}
	else if (!strncmp(token, "resdom", 6))
	{
		return_value = (LispValue) _readResdom();
		return_value->type = T_Resdom;
	}
	else if (!strncmp(token, "expr", 4))
	{
		return_value = (LispValue) _readExpr();
		return_value->type = T_Expr;
	}
	else if (!strncmp(token, "var", 3))
	{
		return_value = (LispValue) _readVar();
		return_value->type = T_Var;
	}
	else if (!strncmp(token, "const", 5))
	{
		return_value = (LispValue) _readConst();
		return_value->type = T_Const;
	}
	else if (!strncmp(token, "func", 4))
	{
		return_value = (LispValue) _readFunc();
		return_value->type = T_Func;
	}
	else if (!strncmp(token, "oper", 4))
	{
		return_value = (LispValue) _readOper();
		return_value->type = T_Oper;
	}
	else if (!strncmp(token, "param", 5))
	{
		return_value = (LispValue) _readParam();
		return_value->type = T_Param;
	}
	else if (!strncmp(token, "estate", 6))
	{
		return_value = (LispValue) _readEState();
		return_value->type = T_EState;
	}
	else if (!strncmp(token, "rel", 3))
	{
		return_value = (LispValue) _readRel();
		return_value->type = T_Rel;
	}
	else if (!strncmp(token, "sortkey", 7))
	{
		return_value = (LispValue) _readSortKey();
		return_value->type = T_SortKey;
	}
	else if (!strncmp(token, "path", 4))
	{
		return_value = (LispValue) _readPath();
		return_value->type = T_Path;
	}
	else if (!strncmp(token, "indexpath", 9))
	{
		return_value = (LispValue) _readIndexPath();
		return_value->type = T_IndexPath;
	}
	else if (!strncmp(token, "joinpath", 8))
	{
		return_value = (LispValue) _readJoinPath();
		return_value->type = T_JoinPath;
	}
	else if (!strncmp(token, "mergepath", 9))
	{
		return_value = (LispValue) _readMergePath();
		return_value->type = T_MergePath;
	}
	else if (!strncmp(token, "hashpath", 8))
	{
		return_value = (LispValue) _readHashPath();
		return_value->type = T_HashPath;
	}
	else if (!strncmp(token, "orderkey", 8))
	{
		return_value = (LispValue) _readOrderKey();
		return_value->type = T_OrderKey;
	}
	else if (!strncmp(token, "joinkey", 7))
	{
		return_value = (LispValue) _readJoinKey();
		return_value->type = T_JoinKey;
	}
	else if (!strncmp(token, "mergeorder", 10))
	{
		return_value = (LispValue) _readMergeOrder();
		return_value->type = T_MergeOrder;
	}
	else if (!strncmp(token, "cinfo", 5))
	{
		return_value = (LispValue) _readCInfo();
		return_value->type = T_CInfo;
	}
	else if (!strncmp(token, "joinmethod", 10))
	{
		return_value = (LispValue) _readJoinMethod();
		return_value->type = T_JoinMethod;
	}
	else if (!strncmp(token, "jinfo", 5))
	{
		return_value = (LispValue) _readJInfo();
		return_value->type = T_JInfo;
	}
	else if (!strncmp(token, "recursive", 9))
	{
		return_value = (LispValue) _readRecursive();
		return_value->type = T_Recursive;
	}
	else
	{
		fprintf(stderr, "badly formatted planstring\n");
		return_value = (LispValue) NULL;
	}
	return(return_value);
}
/*------------------------------------------------------------*/

/* ----------------
 *	readValue
 *
 * given a string representation of the value of the given type,
 * create the appropriate Datum
 * ----------------
 */
Datum
readValue(type)
    ObjectId type;
{
    int 	length;
    int 	tokenLength;
    char 	*token;
    Boolean	byValue;
    Datum 	res;
    char 	*s;
    int 	i;

    byValue	= get_typbyval(type);

    /*
     * read the actual length of the value
     */
    token = lsptok(NULL, &tokenLength);
    length = atoi(token);
    token = lsptok(NULL, &tokenLength);	/* skip the '{' */

    if (byValue) {
	if (length > sizeof(Datum)) {
	    elog(WARN, "readValue: byval & length = %d", length);
	}
	s = (char *) (&res);
	for (i=0; i<sizeof(Datum); i++) {
	    token = lsptok(NULL, &tokenLength);
	    s[i] = (char) atoi(token);
	}
    } else if (length <= 0) {
	s = NULL;
    } else if (length >= 1) {
	s = palloc(length);
	Assert( s!=NULL );
	for (i=0; i<length; i++) {
	    token = lsptok(NULL, &tokenLength);
	    s[i] = (char) atoi(token);
	}
	res = PointerGetDatum(s);
    }

    token = lsptok(NULL, &tokenLength);	/* skip the '}' */
    if (token[0] != '}') {
	elog(WARN, "readValue: '}' expected, length =%d", length);
    }

    return(res);
}
