/*
 *  READFUNCS.C -- Reader functions for Postgres tree nodes.
 *
 *	These routines read in and allocate space for plans.
 *	The main function is at the bottom and figures out what particular
 *  function to use.
 *
 *  All these functions assume that lsptok already has its string.
 */


#include <stdio.h>
#include <math.h>
#include <strings.h>

#include "c.h"
#include "datum.h"
#include "pg_lisp.h"
#include "nodes.h"
#include "primnodes.h"
#include "primnodes.a.h"
#include "plannodes.h"
#include "execnodes.h"
#include "execnodes.a.h"
#include "relation.h"
#include "tags.h"


extern LispValue lispRead();
extern char *lsptok ARGS((char *string, int *length));

void
_getPlan(node)

Plan node;

{
	char *token;
	int length;

	token = lsptok(NULL, &length);    	/* first token is :cost */
	token = lsptok(NULL, &length);    	/* next is the actual cost */

	node->cost = (Cost) atof(token);

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

	token = lsptok(NULL, &length);    	/* eat :qpqpal */

	node->qpqual = lispRead(true);

	token = lsptok(NULL, &length);    	/* eat :lefttree */

	node->lefttree = (struct Plan *) lispRead(true);

	token = lsptok(NULL, &length);    	/* eat :righttree */

	node->righttree = (struct Plan *) lispRead(true);

}

/*
 *  Stuff from plannodes.h
 */

Plan _readPlan()
{
	char *token;
	int length;
	Plan local_node;

	local_node = (Plan) palloc(sizeof(struct _Plan));
	_getPlan(local_node);
	local_node->printFunc = PrintPlan;
	local_node->equalFunc = EqualPlan;

	return( local_node );
}


/*
 * Does some obscene, possibly unportable, magic with sizes of things.
 */

Result _readResult()

{
	Result	local_node;
	char *token;
	int length;

	local_node = (Result) palloc(sizeof(struct _Result));
	_getPlan(local_node);

	token = lsptok(NULL, &length);    	/* eat :resrellevelqual */

	local_node->resrellevelqual = lispRead(true);  /* now read it */

	token = lsptok(NULL, &length);    	/* eat :resconstantqual */

	local_node->resconstantqual = lispRead(true);	/* now read it */
	local_node->printFunc = PrintResult;
	local_node->equalFunc = EqualResult;

	return( local_node );
}

/*
 *  Existential is a subclass of Plan.
 *
 *  In fact, it is identical.
 */

Existential _readExistential()

{
	Existential	local_node;

	local_node = (Existential) palloc(sizeof(struct _Existential));

	_getPlan(local_node);

	local_node->printFunc = PrintExistential;
	local_node->equalFunc = EqualExistential;

	return( local_node );
}

/*
 *  Append is a subclass of Plan.
 */

Append _readAppend()

{
	Append local_node;
	char *token;
	int length;

	local_node = (Append) palloc(sizeof(struct _Append));

	_getPlan(local_node);

	token = lsptok(NULL, &length);    		/* eat :unionplans */

	local_node->unionplans = lispRead(true); 		/* now read it */
	
	token = lsptok(NULL, &length);    		/* eat :unionrelid */
	token = lsptok(NULL, &length);    		/* get unionrelid */

	local_node->unionrelid = atoi(token);

	token = lsptok(NULL, &length);    	/* eat :unionrtentries */

	local_node->unionrtentries = lispRead(true);	/* now read it */

	local_node->printFunc = PrintAppend;
	local_node->equalFunc = EqualAppend;
	return(local_node);
}

/*
 * In case Join is not the same structure as Plan someday.
 */

void _getJoin(node)

Join node;

{
	_getPlan(node);
}

/*
 *  Join is a subclass of Plan
 */

Join _readJoin()

{
	Join	local_node;

	local_node = (Join) palloc(sizeof(struct _Join));

	_getJoin(local_node);
	local_node->printFunc = PrintJoin;
	local_node->equalFunc = EqualJoin;
	return( local_node );
}

/*
 *  NestLoop is a subclass of Join
 */

NestLoop _readNestLoop()

{
	NestLoop	local_node;

	local_node = (NestLoop) palloc(sizeof(struct _NestLoop));

	_getJoin(local_node);
	local_node->printFunc = PrintNestLoop;
	local_node->equalFunc = EqualNestLoop;
	return( local_node );
}

/*
 *  MergeSort is a subclass of Join
 */

MergeSort _readMergeSort(node)

{
	MergeSort	local_node;
	char		*token;
	int length;

	local_node = (MergeSort) palloc(sizeof(struct _MergeSort));

	_getJoin(local_node);
	token = lsptok(NULL, &length);    		/* eat :mergeclauses */
	
	local_node->mergeclauses = lispRead(true);		/* now read it */

	token = lsptok(NULL, &length);    		/* eat :mergesortop */

	token = lsptok(NULL, &length);    		/* get mergesortop */

	local_node->mergesortop = atoi(token);

	local_node->printFunc = PrintMergeSort;
	local_node->equalFunc = EqualMergeSort;
	
	return( local_node );
}

/*
 *  HashJoin is a subclass of Join.
 */

HashJoin _readHashJoin()

{
	HashJoin	local_node;
	char 		*token;
	int length;

	local_node = (HashJoin) palloc(sizeof(struct _HashJoin));

	_getJoin(local_node);

	token = lsptok(NULL, &length);    		/* eat :hashclauses */
	
	local_node->hashclauses = lispRead(true);	 	/* now read it */

	token = lsptok(NULL, &length);    		/* eat :hashjoinop */

	token = lsptok(NULL, &length);    		/* get hashjoinop */

	local_node->hashjoinop = atoi(token);
	local_node->printFunc = PrintHashJoin;
	local_node->equalFunc = EqualHashJoin;
	
	return( local_node );
}

/*
 *  Scan is a subclass of Node
 *  (Actually, according to the plannodes.h include file, it is a
 *  subclass of Plan.  This is why _getPlan is used here.)
 */

/*
 * Scan gets its own get function since stuff inherits it.
 */

void _getScan(node)

Scan node;

{
	char *token;
	int length;

	_getPlan(node);
	
	token = lsptok(NULL, &length);    		/* eat :scanrelid */

	token = lsptok(NULL, &length);    		/* get scanrelid */

	node->scanrelid = atoi(token);
}

/*
 * Scan is a subclass of Plan (Not Node, see above).
 */

Scan _readScan()

{
	Scan 		local_node;

	local_node = (Scan) palloc(sizeof(struct _Scan));

	_getScan(local_node);
	local_node->printFunc = PrintScan;
	local_node->equalFunc = EqualScan;

	return(local_node);
}

/*
 *  SeqScan is a subclass of Scan
 */

SeqScan _readSeqScan()

{
	SeqScan 	local_node;

	local_node = (SeqScan) palloc(sizeof(struct _SeqScan));

	_getScan(local_node);
	local_node->printFunc = PrintSeqScan;
	local_node->equalFunc = EqualSeqScan;

	return(local_node);
}

/*
 *  IndexScan is a subclass of Scan
 */

IndexScan _readIndexScan()

{
	IndexScan	local_node;
	char		*token;
	int length;

	local_node = (IndexScan) palloc(sizeof(struct _IndexScan));

	_getScan(local_node);

	
	token = lsptok(NULL, &length);    		/* eat :indxid */

	local_node->indxid = lispRead(true);		/* now read it */
	
	token = lsptok(NULL, &length);    		/* eat :indxqual */

	local_node->indxqual = lispRead(true); 		/* now read it */
	local_node->printFunc = PrintIndexScan;
	local_node->equalFunc = EqualIndexScan;

	return(local_node);
}

/*
 *  Temp is a subclass of Plan
 */

Temp _readTemp()

{
	Temp		local_node;
	char		*token;
	int length;

	local_node = (Temp) palloc(sizeof(struct _Temp));

	_getPlan(local_node);

	token = lsptok(NULL, &length);    		/* eat :tempid */
	token = lsptok(NULL, &length);    		/* get tempid */

	local_node->tempid = atoi(token);

	token = lsptok(NULL, &length);    		/* eat :keycount */

	token = lsptok(NULL, &length);    		/* get keycount */

	local_node->keycount = atoi(token);

	local_node->printFunc = PrintTemp;
	local_node->equalFunc = EqualTemp;
	return(local_node);
}

/*
 *  Sort is a subclass of Temp
 */
Sort _readSort()

{
	Sort		local_node;
	char		*token;
	int length;

	local_node = (Sort) palloc(sizeof(struct _Sort));

	_getPlan(local_node);

	token = lsptok(NULL, &length);    		/* eat :tempid */
	token = lsptok(NULL, &length);    		/* get tempid */

	local_node->tempid = atoi(token);

	token = lsptok(NULL, &length);    		/* eat :keycount */

	token = lsptok(NULL, &length);    		/* get keycount */

	local_node->keycount = atoi(token);
	local_node->printFunc = PrintSort;
	local_node->equalFunc = EqualSort;

	return(local_node);
}

/*
 *  Hash is a subclass of Temp
 */

Hash _readHash()

{
	Hash		local_node;
	char		*token;
	int length;

	local_node = (Hash) palloc(sizeof(struct _Hash));

	_getPlan(local_node);

	token = lsptok(NULL, &length);    		/* eat :tempid */
	token = lsptok(NULL, &length);    		/* get tempid */

	local_node->tempid = atoi(token);

	token = lsptok(NULL, &length);    		/* eat :keycount */

	token = lsptok(NULL, &length);    		/* get keycount */

	local_node->keycount = atoi(token);
	local_node->printFunc = PrintHash;
	local_node->equalFunc = EqualHash;

	return(local_node);
}

/*
 *  Stuff from primnodes.h.
*/

/*
 *  Resdom is a subclass of Node
 */

Resdom _readResdom()

{
	Resdom		local_node;
	char		*token;
	int length;

	local_node = (Resdom) palloc(sizeof(struct _Resdom));

	token = lsptok(NULL, &length);    		/* eat :resno */
	token = lsptok(NULL, &length);    		/* get resno */

	local_node->resno = atoi(token);

	token = lsptok(NULL, &length);    		/* eat :restype */
	token = lsptok(NULL, &length);    		/* get restype */

	local_node->restype = atoi(token);

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

		local_node->resname = (Name) palloc(sizeof(Char16Data));
		strcpy(local_node->resname, token);
		token[length - 2] = '\"';
	}

	token = lsptok(NULL, &length);    		/* eat :reskey */
	token = lsptok(NULL, &length);    		/* get reskey */

	local_node->reskey = atoi(token);

	token = lsptok(NULL, &length);    		/* eat :reskeyop */

	token = lsptok(NULL, &length);    		/* get reskeyop */

	local_node->reskeyop = (OperatorTupleForm) atoi(token);
	local_node->printFunc = PrintResdom;
	local_node->equalFunc = EqualResdom;

	return(local_node);
}

/*
 *  Expr is a subclass of Node
 */

Expr _readExpr()

{
	Expr local_node;

	local_node = (Expr) palloc(sizeof(struct _Expr));

	local_node->printFunc = PrintExpr;
	local_node->equalFunc = EqualExpr;
	return(local_node);
}

/*
 *  Var is a subclass of Expr
 */

Var _readVar()

{
	Var		local_node;
	char		*token;
	int length;

	local_node = (Var) palloc(sizeof(struct _Var));

	token = lsptok(NULL, &length);    		/* eat :varno */
	token = lsptok(NULL, &length);    		/* get varno */
	
	local_node->varno = atoi(token);

	token = lsptok(NULL, &length);    		/* eat :varattno */
	token = lsptok(NULL, &length);    		/* get varattno */
	
	local_node->varattno = atoi(token);

	token = lsptok(NULL, &length);    		/* eat :vartype */
	token = lsptok(NULL, &length);    		/* get vartype */
	
	local_node->vartype = atoi(token);

	token = lsptok(NULL, &length);    		/* eat :vardotfields */


	/* What to do about the extra ")"'s??? */

	local_node->vardotfields = lispRead(true); 	/* now read it */

	token = lsptok(NULL, &length);    		/* eat :vararrayindex */
	token = lsptok(NULL, &length);    		/* get vararrayindex */
	
	local_node->vararrayindex = atoi(token);

	token = lsptok(NULL, &length);    		/* eat :varid */

	local_node->varid = lispRead(true); 		/* now read it */
	/* token = lsptok(NULL, &length); */    		/* eat last ) */
	local_node->printFunc = PrintVar;
	local_node->equalFunc = EqualVar;

	return(local_node);
}

/*
 *  Const is a subclass of Expr
 */

Const _readConst()

{
	Const	local_node;
	char *token;
	int length;

	local_node = (Const) palloc(sizeof(struct _Const));

	token = lsptok(NULL, &length);      /* get :consttype */
	token = lsptok(NULL, &length);      /* now read it */

	local_node->consttype = atoi(token);


	token = lsptok(NULL, &length);      /* get :constlen */
	token = lsptok(NULL, &length);      /* now read it */

	local_node->constlen = atoi(token);

	token = lsptok(NULL, &length);      /* get :constvalue */
	token = lsptok(NULL, &length);      /* now read it */

	local_node->constvalue = Int32GetDatum(atoi(token));

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
	local_node->printFunc = PrintConst;
	local_node->equalFunc = EqualConst;
	return(local_node);
}

/*
 *  Func is a subclass of Expr
 */

Func _readFunc()

{
	Func	local_node;
	char *token;
	int length;

	local_node = (Func) palloc(sizeof(struct _Func));

	token = lsptok(NULL, &length);      /* get :funcid */
	token = lsptok(NULL, &length);      /* now read it */

	local_node->funcid = atoi(token);

	token = lsptok(NULL, &length);      /* get :functype */
	token = lsptok(NULL, &length);      /* now read it */

	local_node->functype = atoi(token);

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
	local_node->printFunc = PrintFunc;
	local_node->equalFunc = EqualFunc;
	return(local_node);
}

/*
 *  Oper is a subclass of Expr
 */

Oper _readOper()

{
	Oper	local_node;
	char 	*token;
	int length;

	local_node = (Oper) palloc(sizeof(struct _Oper));

	token = lsptok(NULL, &length);      /* get :opno */
	token = lsptok(NULL, &length);      /* now read it */

	local_node->opno = atoi(token);

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

	local_node->printFunc = PrintOper;
	local_node->equalFunc = EqualOper;
	return(local_node);
}

/*
 *  Param is a subclass of Expr
 */

Param _readParam()

{
	Param	local_node;
	char 	*token;
	int length;

	local_node = (Param) palloc(sizeof(struct _Param));

	token = lsptok(NULL, &length);      /* get :paramid */
	token = lsptok(NULL, &length);      /* now read it */

	local_node->paramid = atoi(token);

	token = lsptok(NULL, &length);      /* get :paramname */
	token = lsptok(NULL, &length);      /* now read it */
	token++;
	token[length - 2] = '\0';

	local_node->paramname = (Name) palloc(sizeof(Char16Data));
	strcpy(local_node->paramname, token);
	token[length - 2] = '\"';

	token = lsptok(NULL, &length);      /* get :paramtype */
	token = lsptok(NULL, &length);      /* now read it */

	local_node->paramtype = atoi(token);
	local_node->printFunc = PrintParam;
	local_node->equalFunc = EqualParam;
	return(local_node);
}

/*
 *  Stuff from execnodes.h
 */

/*
 *  EState is a subclass of Node.
 */

EState _readEState()

{
	EState	local_node;
	char *token;
	int length;

	local_node = (EState) palloc(sizeof(struct _EState));

	token = lsptok(NULL, &length);      /* get :direction */
	token = lsptok(NULL, &length);      /* now read it */

	local_node->es_direction = atoi(token);

	token = lsptok(NULL, &length);      /* get :time */
	token = lsptok(NULL, &length);      /* now read it */

	local_node->es_time = (unsigned long) atoi(token);

	token = lsptok(NULL, &length);      /* get :owner */
	token = lsptok(NULL, &length);      /* now read it */

	local_node->es_owner = atoi(token);

	token = lsptok(NULL, &length);      /* get :locks */

	local_node->es_locks = lispRead(true);   /* now read it */

	token = lsptok(NULL, &length);      /* get :subplan_info */

	local_node->es_subplan_info = lispRead(true); /* now read it */

	token = lsptok(NULL, &length);      /* get :error_message */
	token = lsptok(NULL, &length);      /* now read it */
	token++;
	token[length-2] = '\0';

	local_node->es_error_message = (Name) palloc(sizeof(Char16Data));
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

	token = lsptok(NULL, &length);      /* get :relation_relation_descriptor */
	token = lsptok(NULL, &length);      /* get @ */
	token = lsptok(NULL, &length);      /* now read it */

	sscanf(token, "%x", &local_node->es_relation_relation_descriptor);

	token = lsptok(NULL, &length);      /* get :result_relation_info */
	token = lsptok(NULL, &length);      /* get @ */
	token = lsptok(NULL, &length);      /* now read it */

	sscanf(token, "%x", &local_node->es_result_relation_info);
	local_node->printFunc = PrintEState;
	local_node->equalFunc = EqualEState;
	return(local_node);
}

/*
 *  Stuff from relation.h
 */

Rel _readRel()

{
	Rel	local_node;
	char 	*token;
	int length;

	local_node = (Rel) palloc(sizeof(struct _Rel));

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
	local_node->printFunc = PrintRel;
	local_node->equalFunc = EqualRel;
	return(local_node);
}

/*
 *  TLE is a subclass of Node.
 */
/*
TLE _readTLE()

{
	TLE	local_node;
	char	*token;
	int length;

	local_node = (TLE) palloc(sizeof(struct _TLE));

	token = lsptok(NULL, &length);      

	local_node->resdom = _readResdom();

	token = lsptok(NULL, &length);     
	local_node->expr = _readExpr();
	return(local_node);
}
*/
/*
 *  TL is a subclass of Node.
 */
/*
void
_readTL(node)
	TL	node;
{
	printf("tl ");
	printf(" :entry ");
	node->entry->printFunc(node->entry);

	printf(" :joinlist ");
	lispDisplay(node->joinlist,0);
}
*/

/*
 *  SortKey is a subclass of Node.
 */

SortKey _readSortKey()

{
	SortKey		local_node;
	char		*token;
	int length;

	local_node = (SortKey) palloc(sizeof(struct _SortKey));
	token = lsptok(NULL, &length);      /* get :varkeys */
	local_node->varkeys = lispRead(true); /* now read it */

	token = lsptok(NULL, &length);      /* get :sortkeys */
	local_node->sortkeys = lispRead(true); /* now read it */

	token = lsptok(NULL, &length);      /* get :relid */
	local_node->relid = lispRead(true);      /* now read it */

	token = lsptok(NULL, &length);      /* get :sortorder */
	local_node->sortorder = lispRead(true);      /* now read it */
	
	local_node->printFunc = PrintSortKey;
	local_node->equalFunc = EqualSortKey;
	return(local_node);
}

/*
 *  Path is a subclass of Node.
 */

Path _readPath()

{
	Path	local_node;
	char 	*token;
	int length;

	local_node = (Path) palloc(sizeof(struct _Path));

	token = lsptok(NULL, &length);      /* get :pathtype */
	token = lsptok(NULL, &length);      /* now read it */

	local_node->pathtype = atoi(token);

	token = lsptok(NULL, &length);      /* get :cost */
	token = lsptok(NULL, &length);      /* now read it */

	local_node->path_cost = (Cost) atof(token);

	token = lsptok(NULL, &length);      /* get :p_ordering */
	local_node->p_ordering = lispRead(true);      /* now read it */

	token = lsptok(NULL, &length);      /* get :keys */
	local_node->keys = lispRead(true);       /* now read it */

	token = lsptok(NULL, &length);      /* get :sortpath */
	local_node->sortpath = (SortKey) lispRead(true);
	local_node->printFunc = PrintPath;
	local_node->equalFunc = EqualPath;

	return(local_node);
}

/*
 *  IndexPath is a subclass of Path.
 */

IndexPath _readIndexPath()

{
	IndexPath	local_node;
	char		*token;
	int length;

	local_node = (IndexPath) palloc(sizeof(struct _IndexPath));

	token = lsptok(NULL, &length);      /* get :pathtype */
	token = lsptok(NULL, &length);      /* now read it */

	local_node->pathtype = atoi(token);

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
	token = lsptok(NULL, &length);      /* now read it */

	local_node->indexid = atoi(token);

	token = lsptok(NULL, &length);      /* get :indexqual */
	local_node->indexqual = lispRead(true);      /* now read it */
	local_node->printFunc = PrintIndexPath;
	local_node->equalFunc = EqualIndexPath;

	return(local_node);
}

/*
 *  JoinPath is a subclass of Path
 */

JoinPath _readJoinPath()

{
	JoinPath	local_node;
	char		*token;
	int length;


	local_node = (JoinPath) palloc(sizeof(struct _JoinPath));

	token = lsptok(NULL, &length);      /* get :pathtype */
	token = lsptok(NULL, &length);      /* now read it */

	local_node->pathtype = atoi(token);

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
	local_node->pathclauseinfo = lispRead(true);           /* now read it */

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

	local_node->printFunc = PrintJoinPath;
	local_node->equalFunc = EqualJoinPath;
	return(local_node);
}

/*
 *  MergePath is a subclass of JoinPath.
 */

MergePath _readMergePath()

{
	MergePath	local_node;
	char 		*token;
	int length;

	local_node = (MergePath) palloc(sizeof(struct _MergePath));

	token = lsptok(NULL, &length);      /* get :pathtype */
	token = lsptok(NULL, &length);      /* now read it */

	local_node->pathtype = atoi(token);

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
	local_node->pathclauseinfo = lispRead(true);           /* now read it */

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
	local_node->path_mergeclauses = lispRead(true);           /* now read it */

	token = lsptok(NULL, &length);      /* get :outersortkeys */
	local_node->outersortkeys = lispRead(true);           /* now read it */

	token = lsptok(NULL, &length);      /* get :innersortkeys */
	local_node->innersortkeys = lispRead(true);           /* now read it */

	local_node->printFunc = PrintMergePath;
	local_node->equalFunc = EqualMergePath;
	return(local_node);
}

/*
 *  HashPath is a subclass of JoinPath.
 */

HashPath _readHashPath()

{
	HashPath	local_node;
	char 		*token;
	int length;

	local_node = (HashPath) palloc(sizeof(struct _HashPath));

	token = lsptok(NULL, &length);      /* get :pathtype */
	token = lsptok(NULL, &length);      /* now read it */

	local_node->pathtype = atoi(token);

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
	local_node->printFunc = PrintHashPath;
	local_node->equalFunc = EqualHashPath;

	return(local_node);
}

/*
 *  OrderKey is a subclass of Node.
 */

OrderKey _readOrderKey()

{
	OrderKey	local_node;
	char		*token;
	int length;

	local_node = (OrderKey) palloc(sizeof(struct _OrderKey));

	token = lsptok(NULL, &length);      /* get :attribute_number */
	token = lsptok(NULL, &length);      /* now read it */

	local_node->attribute_number = atoi(token);
	
	token = lsptok(NULL, &length);      /* get :array_index */
	token = lsptok(NULL, &length);      /* now read it */

	local_node->array_index = atoi(token);

	local_node->printFunc = PrintOrderKey;
	local_node->equalFunc = EqualOrderKey;
	return(local_node);
}

/*
 *  JoinKey is a subclass of Node.
 */

JoinKey _readJoinKey()

{
	JoinKey		local_node;
	char		*token;
	int length;

	local_node = (JoinKey) palloc(sizeof(struct _JoinKey));

	token = lsptok(NULL, &length);      /* get :outer */
	local_node->outer = lispRead(true);      /* now read it */

	token = lsptok(NULL, &length);      /* get :inner */
	local_node->inner = lispRead(true);      /* now read it */
	
	local_node->printFunc = PrintJoinKey;
	local_node->equalFunc = EqualJoinKey;
	return(local_node);
}

/*
 *  MergeOrder is a subclass of Node.
 */

MergeOrder _readMergeOrder()

{
	MergeOrder	local_node;
	char		*token;
	int length;

	token = lsptok(NULL, &length);      /* get :join_operator */
	token = lsptok(NULL, &length);      /* now read it */

	local_node->join_operator = atoi(token);

	token = lsptok(NULL, &length);      /* get :left_operator */
	token = lsptok(NULL, &length);      /* now read it */

	local_node->left_operator = atoi(token);

	token = lsptok(NULL, &length);      /* get :right_operator */
	token = lsptok(NULL, &length);      /* now read it */

	local_node->right_operator = atoi(token);

	token = lsptok(NULL, &length);      /* get :left_type */
	token = lsptok(NULL, &length);      /* now read it */

	local_node->left_type = atoi(token);

	token = lsptok(NULL, &length);      /* get :right_type */
	token = lsptok(NULL, &length);      /* now read it */

	local_node->right_type = atoi(token);

	local_node->printFunc = PrintMergeOrder;
	local_node->equalFunc = EqualMergeOrder;
	return(local_node);
}

/*
 *  CInfo is a subclass of Node.
 */

CInfo _readCInfo()

{
	CInfo	local_node;
	char 	*token;
	int length;

	local_node = (CInfo) palloc(sizeof(struct _CInfo));

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

	local_node->hashjoinoperator = atoi(token);

	local_node->printFunc = PrintCInfo;
	local_node->equalFunc = EqualCInfo;
	return(local_node);
}

/*
 *  JoinMethod is a subclass of Node.
 */

JoinMethod _readJoinMethod()

{
     	JoinMethod 	local_node;
	char		*token;
	int length;

	local_node = (JoinMethod) palloc(sizeof(struct _JoinMethod));

	token = lsptok(NULL, &length);      /* get :jmkeys */
	local_node->jmkeys = lispRead(true);/* now read it */

	token = lsptok(NULL, &length);      /* get :clauses */
	local_node->clauses = lispRead(true); /* now read it */

	local_node->printFunc = PrintJoinMethod;
	local_node->equalFunc = NULL; /* EqualJoinMethod; */
	return(local_node);
}

/*
 * HInfo is a subclass of JoinMethod.
 */

HInfo _readHInfo()

{
	HInfo 	local_node;
	char 	*token;
	int length;

	local_node = (HInfo) palloc(sizeof(struct _HInfo));

	token = lsptok(NULL, &length);      /* get :hashop */
	token = lsptok(NULL, &length);      /* now read it */
	
	local_node->hashop = atoi(token);

	token = lsptok(NULL, &length);      /* get :jmkeys */
	local_node->jmkeys = lispRead(true); /* now read it */

	token = lsptok(NULL, &length);      /* get :clauses */
	local_node->clauses = lispRead(true);           /* now read it */

	local_node->printFunc = PrintHInfo;
	local_node->equalFunc = EqualHInfo;
	return(local_node);
}

/*
 *  JInfo is a subclass of Node.
 */

JInfo _readJInfo()

{
	JInfo	local_node;
	char	*token;
	int length;

	local_node = (JInfo) palloc(sizeof(struct _JInfo));

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
	local_node->printFunc = PrintJInfo;
	local_node->equalFunc = EqualJInfo;
	return(local_node);
}


RuleLockNode _readRuleLockNode()

{
	RuleLockNode	local_node;
	char 		*token;
	int length;

	local_node = (RuleLockNode) palloc(sizeof(struct _RuleLockNode));

	token = lsptok(NULL, &length);      /* get :type */
	token = lsptok(NULL, &length);      /* now read it */

	local_node->rltype = (LockType) atoi(token);

	token = lsptok(NULL, &length);      /* get :relation */
	token = lsptok(NULL, &length);      /* now read it */

	local_node->rlrelation = atoi(token);
    
	token = lsptok(NULL, &length);      /* get :attribute */
	token = lsptok(NULL, &length);      /* now read it */

	local_node->rlattribute = atoi(token);
    
	token = lsptok(NULL, &length);      /* get :var */
	local_node->rlvar = (Var) lispRead(true);

	token = lsptok(NULL, &length);      /* get :plan */
	local_node->rlplan = lispRead(true); /* now read it */

	local_node->printFunc = PrintRuleLockNode;
	local_node->equalFunc = NULL; /* EqualRuleLockNode; */
	return(local_node);
}

/* 
 * Given a character string containing a plan, parsePlanString sets up the
 * plan structure representing that plan.
 *
 * The string passed to parsePlanString must be null-terminated.
 */

LispValue parsePlanString()


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
	else if (!strncmp(token, "mergesort", 9))
	{
		return_value = (LispValue) _readMergeSort();
		return_value->type = T_MergeSort;
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
	else if (!strncmp(token, "rulelock", 8))
	{
		return_value = (LispValue) _readRuleLockNode();
		return_value->type = T_RuleLockNode;
	}
	else
	{
		fprintf(stderr, "badly formatted planstring\n");
		return_value = (LispValue) NULL;
	}
	return(return_value);
}
