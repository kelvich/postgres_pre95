

/* ----------------------------------------------------------------
 * $Header$
 * ----------------------------------------------------------------
 */
/****************************************************/

#include "tmp/postgres.h"
#include "utils/log.h"
#include "utils/rel.h"
#include "nodes/pg_lisp.h"		/* for LispValue and lisp support */
#include "nodes/primnodes.h"
#include "nodes/primnodes.a.h"		/* for accessors to varnodes etc */

#include "rules/prs2.h"			/* XXX - temporarily */
/* #include "rules/prs2locks.h"		 XXX - temporarily */

#include "parser/parse.h"		/* for RETRIEVE,REPLACE,APPEND ... */
#include "parser/parsetree.h"		/* for parsetree manipulation */

#include "./RewriteSupport.h"	
#include "./RewriteDebug.h"
#include "./RewriteHandler.h"
#include "./RewriteManip.h"
#include "./locks.h"

/*
 * Gather meta information about parsetree, and rule
 * Fix rule body and qualifier so that they can be mixed
 * with the parsetree and maintain semantic validity
 */

extern List lispCopyList();
extern List lispCopy();


extern int DebugLvl;
RewriteInfo *GatherRewriteMeta(parsetree, rule_action, rule_qual, rt_index, event, instead_flag)
     List parsetree,rule_action, rule_qual;
     int rt_index, event, *instead_flag;
{
    RewriteInfo *info;    
    int rt_length;
    List result_reln;

    info = (RewriteInfo *) palloc(sizeof(RewriteInfo));
    info->rt_index = rt_index;
    info->event = event;
    info->instead_flag = *instead_flag;
    info->rule_action = rule_action;
    info->rule_qual = (List) CopyObject(rule_qual);
    info->nothing = FALSE;
    info->action = root_command_type(parse_root(info->rule_action));
    if (info->rule_action == LispNil) info->nothing = TRUE;
    if (info->nothing) return info;
    info->current_varno = rt_index;
    info->rt = lispCopyList(root_rangetable(parse_root(parsetree)));
    rt_length = length(info->rt);
    info->rt = append(info->rt,
		      root_rangetable(parse_root(info->rule_action)));
    info->new_varno = PRS2_NEW_VARNO + rt_length;
    OffsetVarNodes(info->rule_action, rt_length);
    OffsetVarNodes(info->rule_qual, rt_length);
    ChangeVarNodes(info->rule_action, PRS2_CURRENT_VARNO+rt_length, rt_index);
    ChangeVarNodes(info->rule_qual, PRS2_CURRENT_VARNO+rt_length, rt_index);
    
    /*
     * bug here about replace CURRENT  -- sort of
     * replace current is deprecated now so this code shouldn't really
     * need to be so clutzy but.....
     */
   if (info->action != RETRIEVE) {	/* i.e update XXXXX */
       int new_result_reln = 0;
       result_reln = root_result_relation(parse_root(info->rule_action));
       switch (CInteger(result_reln)) {
       case PRS2_CURRENT_VARNO: new_result_reln = rt_index;
	   break; 
       case PRS2_NEW_VARNO:	/* XXX */
       default            :
	   new_result_reln = CInteger(result_reln) + rt_length;
	   break;
       }
       root_result_relation(parse_root(info->rule_action)) =
	   lispInteger (new_result_reln);
   }
    
    return info;
}



List OptimizeRIRRules(locks)
     List locks;
{
    List attr_level = LispNil,i;
    List relation_level = LispNil;

    foreach (i, locks) {
	Prs2OneLock rule_lock 	= (Prs2OneLock)CAR(i);
	if ( prs2OneLockGetAttributeNumber(rule_lock) == -1) 
	    relation_level = nappend1(relation_level, (LispValue)rule_lock);
	else attr_level = nappend1(attr_level, (LispValue)rule_lock);
    }
    return append(relation_level, attr_level);
}
/*
 * idea is to put instead rules before regular rules so that
 * excess semantically queasy queries aren't processed
 */

List OrderRules(locks)
     List locks;
{
    List regular = LispNil,i;
    List instead_rules = LispNil;

    foreach (i, locks) {
	Prs2OneLock rule_lock 	= (Prs2OneLock)CAR(i);
	if (!(IsActive(rule_lock) && IsRewrite(rule_lock))) continue;
	if (IsInstead(rule_lock))
	    instead_rules = nappend1(instead_rules, (LispValue)rule_lock);
	else regular = nappend1(regular, (LispValue)rule_lock);
    }
    return append(regular, instead_rules);
}
int AllRetrieve(actions)
     List actions;
{
    List n;

    foreach(n, actions) {
        List pt = CAR(n);
	int event;
	List command_type;
	List root = parse_root(pt);

	command_type = root_command_type_atom(root);
	if ((consp(command_type)) && (CInteger(CAR(command_type)) == '*'))
	    event = (int) CAtom(CDR(command_type));
	else
	    event = root_command_type(root);
	if (event != RETRIEVE) return false;
    }
    return true;
}
List StupidUnionRetrieveHack(parsetree, actions)
     List parsetree, actions;
{
    return actions;
}
List FireRetrieveRulesAtQuery(parsetree, rt_index, relation,instead_flag,
			      rule_flag)
     List parsetree;
     Relation relation;
     int rt_index, *instead_flag, rule_flag;
{
    List i;
    List work   =  LispNil;
    List results  = LispNil;
    RuleLock rt_entry_locks = NULL;
    List locks = LispNil;
    rt_entry_locks = prs2GetLocksFromRelation(
				RelationGetRelationName(relation));
    locks = MatchRetrieveLocks(rt_entry_locks, rt_index, parsetree);
    foreach (i, locks) {
	Prs2OneLock rule_lock 	= (Prs2OneLock)CAR(i);
	List rule;
	List rule_action;
	List rule_qual;
	List qual;
	int instead;

	if (!IsInstead(rule_lock)) continue;
	work = nappend1(work, (LispValue)rule_lock);
    }
    if (work != LispNil) {
	work = OptimizeRIRRules(locks);
	foreach (i, work) {
	    Prs2OneLock rule_lock 	= (Prs2OneLock)CAR(i);
	    List rule;
	    int foo;
	    int relation_level;
	    int modified = FALSE;
	    rule = RuleIdGetActionInfo (prs2OneLockGetRuleId(rule_lock),
					&foo);
	    if (null(rule)) continue;
	    relation_level = (prs2OneLockGetAttributeNumber(rule_lock) == -1);
	    if (!CDR(rule)) {
		*instead_flag = TRUE;
		return LispNil;
	    }
	    if (!rule_flag &&
		length(CDR(rule)) >= 2
		&& AllRetrieve(CDR(rule))) {
		*instead_flag = TRUE;
		return StupidUnionRetrieveHack(parsetree, CDR(rule));
	    }
	    ApplyRetrieveRule(parsetree, rule,rt_index,relation_level,
			      prs2OneLockGetAttributeNumber(rule_lock),
			      &modified);
	    if (modified) {
		*instead_flag = TRUE;
		FixResdomTypes(parse_targetlist(parsetree));
		return lispCons(parsetree,LispNil);
	    }
	}
    }
    return LispNil;
}	
	
	
/* Idea is like this:
 *
 * retrieve-instead-retrieve rules have different semantics than update nodes
 * Separate RIR rules from others.  Pass others to FireRules.
 * Order RIR rules and process.
 *
 */

ApplyRetrieveRule(parsetree, rule, rt_index,relation_level, attr_num,modified)
     List parsetree,rule;
     int relation_level, attr_num, rt_index, *modified;
{
    List rule_action;
    List rule_qual, rt;
    int nothing,rt_length;
    int badpostquel= FALSE;
    if (!null(rule) && CDR(rule)) {
	if (length(CDR(rule)) > 1)
	    return;
	rule_action = CAR(CDR(rule));
	rule_qual = CAR(rule);
	nothing = FALSE;
    }
    else nothing = TRUE;
    if (rule_action == LispNil) nothing = TRUE;
    rt = lispCopyList(root_rangetable(parse_root(parsetree)));
    rt_length = length(rt);
    rt = append(rt, root_rangetable(parse_root(rule_action)));
    root_rangetable(parse_root(parsetree)) = rt;
    root_rangetable(parse_root(rule_action)) = rt;
    OffsetVarNodes(rule_action, rt_length);
    OffsetVarNodes(rule_qual, rt_length);
    ChangeVarNodes(rule_action, PRS2_CURRENT_VARNO+rt_length, rt_index);
    ChangeVarNodes(rule_qual, PRS2_CURRENT_VARNO+rt_length, rt_index);
    if (relation_level) {
	HandleViewRule(parsetree, rt, parse_targetlist(rule_action),rt_index
		       ,modified);
    }
    else {
	HandleRIRAttributeRule(parsetree, rt,
			       parse_targetlist(rule_action),
			       rt_index, attr_num,modified,&badpostquel);
    }
    if (*modified && !badpostquel)
	AddQual(parsetree,parse_qualification(rule_action));
}
List ProcessRetrieveQuery(parsetree, rt,instead_flag,rule)
     List parsetree,rt;
     int *instead_flag,rule;
{
    List rt_entry_ptr;
    int rt_index = 0;
    List product_queries = LispNil;

    foreach (rt_entry_ptr, rt) {
	List rt_entry = CAR(rt_entry_ptr);
	Name rt_entry_relname = NULL;
	Relation rt_entry_relation = NULL;
	RuleLock rt_entry_locks = NULL;
	ObjectId rt_entry_relid = NULL;
	int just_rir_rules = FALSE;
 	List new_rewritten  = NULL;
	List result = LispNil;

	rt_entry_relname = (Name) CString(CADR(rt_entry));
	rt_index++;
	rt_entry_relation = amopenr(rt_entry_relname);
	result = 
	    FireRetrieveRulesAtQuery(parsetree, rt_index, rt_entry_relation,
				     instead_flag,rule);
	amclose(rt_entry_relation);
	if (*instead_flag && result) return result;
	if (*instead_flag) return LispNil;
    }
    if (rule) return LispNil;
    foreach (rt_entry_ptr, rt) {
	List rt_entry = CAR(rt_entry_ptr);
	Name rt_entry_relname = NULL;
	Relation rt_entry_relation = NULL;
	RuleLock rt_entry_locks = NULL;
	ObjectId rt_entry_relid = NULL;
	int just_rir_rules = FALSE;
 	List new_rewritten  = NULL;
	List result = LispNil;
	List locks = LispNil;
	List dummy_products;
	rt_index++;
	rt_entry_relname = (Name) CString(CADR(rt_entry));
	rt_entry_relation = amopenr(rt_entry_relname);
	rt_entry_locks = prs2GetLocksFromRelation(
			  RelationGetRelationName(rt_entry_relation));
	
	amclose(rt_entry_relation);
	locks = MatchLocks(EventIsRetrieve,rt_entry_locks,rt_index,parsetree);
	if (locks == LispNil) continue;
	result = FireRules(parsetree, rt_index, RETRIEVE,
			   instead_flag, locks,&dummy_products);
	if (*instead_flag) return nappend1(LispNil, result);
	if (result != LispNil)
	    product_queries = append(product_queries, result);
    }
    return product_queries;
}

List CopyAndAddQual(parsetree, actions, rule_qual,
		    rt_index,event)
     List parsetree,rule_qual,actions;
     int event, rt_index;
{
    List new_pt = (List) CopyObject(parsetree);
    List new_qual = LispNil;
    List rule_action = LispNil;

    if (actions)
	rule_action = CAR(actions);
    if (rule_qual != LispNil)
	new_qual = (List) CopyObject(rule_qual);
    if (rule_action != LispNil) {
	List rt;
	int rt_length;

	rt = root_rangetable(parse_root(new_pt));
	rt_length = length(rt);
	rt = append(rt,
		    lispCopyList(root_rangetable(parse_root(rule_action))));
	root_rangetable(parse_root(new_pt)) = rt;
	OffsetVarNodes(new_qual, rt_length);
	ChangeVarNodes(new_qual, PRS2_CURRENT_VARNO+rt_length, rt_index);
    }
    /* XXX -- where current doesn't work for instead nothing.... yet*/
    AddNotQual(new_pt, new_qual);
    return new_pt;
}


/*
 *  FireRules(parsetree, rt_index, relation, rt, event,
 *			 instead_flag, locks,qual_products)
 *  
 *  Iterate through rule locks applying rules.  After an instead rule
 *  rule has been applied, return just new parsetree and let RewriteQuery
 *  start the process all over again.  The locks are reordered to maintain
 *  sensible semantics.  remember: reality is for dead birds -- glass
 *
 */

List FireRules(parsetree, rt_index, event,  instead_flag, locks,qual_products)
     List parsetree;
     int event, *instead_flag, rt_index;
     List locks;
     List *qual_products;
{
    RewriteInfo *info;
    List results = LispNil;
    List i;

/* choose rule to fire from list of rules */

    if (locks == LispNil) {
       (void) ProcessRetrieveQuery(parsetree,
				   root_rangetable(parse_root(parsetree)),
				   instead_flag,TRUE);
       if (*instead_flag) return nappend1(LispNil,parsetree);
       else return LispNil;
   }
					
    locks = OrderRules(locks);	/* instead rules first */
    foreach ( i , locks ) {
       Prs2OneLock rule_lock 	= (Prs2OneLock)CAR(i);
       List saved_query = LispNil;
       List rule;
       List qual;
       List event_qual, actions;
       List r;
       int foo;
       *instead_flag = FALSE;
       rule = RuleIdGetActionInfo (prs2OneLockGetRuleId(rule_lock),
				   instead_flag);
       
       /* multiple rule action time */
       if (null(rule)) return LispNil;
       event_qual = CAR(rule);
       actions = CDR(rule);
       if (event_qual != LispNil && *instead_flag)
	   *qual_products =
	       nappend1(*qual_products,
			CopyAndAddQual(parsetree,actions,event_qual,
				       rt_index,event));
       foreach (r, actions) {
	   List rule_action  = CAR(r);
	   List rule_qual = lispCopy(event_qual);
	   
	   /* save range table information 
	    * possibly shift current/new to end of rangetable 
	    * Step 1
	    * Rewrite current.attribute or current to tuple variable
	    * this appears to be done in parser?
	    */

	   info = GatherRewriteMeta(parsetree, rule_action,
				    rule_qual,rt_index,event,instead_flag);
	   
	   /* handle escapable cases, or those handled by other code */
	   
	   if (info->nothing && *instead_flag) return LispNil;
	   if (info->nothing) continue;
	   if (info->action == info->event == RETRIEVE) continue;
	   /*
	    * Event Qualification forces copying of parsetree --- XXX
	    * and splitting into two queries one w/rule_qual, one
	    * w/NOT rule_qual
	    */
	   /*
	    * Also add user query qual onto rule action
	    *
	    */
	   qual = parse_qualification(parsetree);
	   AddQual(info->rule_action, qual);
	   
	   if (info->rule_qual != LispNil) 
	       AddQual(info->rule_action, info->rule_qual);

	   /* Step 2
	    * Rewrite new.attribute w/ right hand side of
	    * target-list entry for appropriate field name in append/replace
	    *
	    */  
	   if ((info->event == APPEND) || (info->event == REPLACE)) {
	       FixNew(info, parsetree);
	   }
	   
	   /* Step 3
	    *
	    *  rewriting due to retrieve rules 
	    */
	   root_rangetable(parse_root(info->rule_action)) = info->rt;       	   
	   (void) ProcessRetrieveQuery(info->rule_action, info->rt, &foo,TRUE);
	   
	   /* Step 4
	    *  
	    * Simplify?
	    * hey, no algorithm for simplification...let the planner do it.
	    */

	   results = nappend1(results,info->rule_action);
       }
       if (*instead_flag) break;
   }
    return results;
}
List ProcessUpdateNode(parsetree, rt_index, event,
			 instead_flag,relation_locks,qual_products)
     List parsetree;
     int event, *instead_flag, rt_index;
     RuleLock relation_locks;
     List *qual_products;
{
    List locks = LispNil;
    Prs2LockType locktype;
    /* First step is to determine which locks actually apply */

    switch(event) {
    case APPEND: locktype = EventIsAppend;
	break;
    case DELETE: locktype = EventIsDelete;
	break;
    case RETRIEVE: locktype = EventIsRetrieve;
	break;
    case REPLACE: locktype = EventIsReplace;
	break;
    default: elog (WARN, "really damm unlikely event");
        break;
    }

    if (relation_locks != NULL)
	locks = MatchLocks(locktype,relation_locks,rt_index,parsetree);
    return FireRules(parsetree, rt_index, event,
		    instead_flag, locks,qual_products);
}

	
	
List RewriteQuery(parsetree,instead_flag,qual_products)
     List parsetree;
     int *instead_flag;
     List *qual_products;
{
    List root              = NULL;
    List command_type      = NULL;
    List rt                = NULL;
    List rt_entry_ptr      = NULL;
    int event              = -1;
    List tl                = NULL;
    List qual              = NULL;
    List something_happened = FALSE;
    List qr_output            = LispNil;
    int rt_index           = 1;
    List product_queries = LispNil;
    List result_relation = LispNil;


    Assert ( parsetree != NULL );
    root = parse_root(parsetree);
    Assert ( root != NULL );
    command_type = root_command_type_atom(root);

    rt	= root_rangetable(root);

    /* if this is a recursive query, command_type node is funny */
    if ((consp(command_type)) && (CInteger(CAR(command_type)) == '*'))
	event = (int) CAtom(CDR(command_type));
    else
        event = root_command_type(root);

    if (DebugLvl) {
	printf("\nRewriteQuery being called with :\n");
	Print_parse ( parsetree );
    }

    /*
     * only for a delete may the targetlist be NULL
     */
    if ( event != DELETE ) {
	tl = parse_targetlist(parsetree);
 	Assert ( tl != NULL );
    }

    result_relation = root_result_relation(parse_root(parsetree));
    if (event != RETRIEVE) {
	List rt_entry;
	Name rt_entry_relname = NULL;
	Relation rt_entry_relation = NULL;
	RuleLock rt_entry_locks = NULL;

	rt_entry = rt_fetch(CInteger(result_relation),
			    root_rangetable(parse_root(parsetree)));
	rt_entry_relname = (Name) CString(CADR(rt_entry));
	rt_entry_relation = amopenr(rt_entry_relname);
	rt_entry_locks = prs2GetLocksFromRelation(
 				RelationGetRelationName(rt_entry_relation));
	amclose(rt_entry_relation);
	product_queries =
	    ProcessUpdateNode(parsetree,
			      CInteger(result_relation),
			      event,
			      instead_flag,
			      rt_entry_locks,qual_products);
	return product_queries;
    }
    else {			/* XXX */
	char *temp;
	List new_pt;
	List other;
	/*
	 * This looks a little foolish.  Something must have been removed
	 * in between these two lines of code.... ?  Commenting these lines
	 * out gives us about an 8% speed up on the wisconsin benchmark.
	 *
	 * temp = PlanToString(parsetree);
	 *
	 * new_pt = (List)StringToPlan(temp);
	 */
	other = (List) CopyObject(parsetree);
	return ProcessRetrieveQuery(other, rt, instead_flag,FALSE);
    }
}

/* rewrite one query via QueryRewrite system, possibly returning 0, or many
 * queries
 */  

List
QueryRewrite ( parsetree )
     List parsetree;
{
    List n;
    List rewritten = LispNil;
    List result = LispNil;
    int instead;
    List qual_products = LispNil;

    instead = FALSE;
    result = RewriteQuery(parsetree, &instead,&qual_products);
    if (!instead) rewritten = nappend1(rewritten, parsetree);
    foreach(n, result) {
        List pt = CAR(n);
        List newstuff = LispNil;

        newstuff = QueryRewrite(pt);
        if (newstuff != LispNil) rewritten = append(rewritten, newstuff);
    }
    if (qual_products != LispNil)
	rewritten = append(rewritten,qual_products);
    if (DebugLvl) {
	puts("printing resulting queries:");
	foreach(n, rewritten) {
	    List pt = CAR(n);
	    Print_parse(pt);
	}
    }
    return rewritten;
}










