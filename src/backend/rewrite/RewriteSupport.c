/* ----------------------------------------------------------------
 *   FILE
 *	RuleHandler.c
 *	
 *   NOTES
 *
 *   IDENTIFICATION
 * $Header$
 * ----------------------------------------------------------------
 */

#include "tmp/postgres.h"
#include "access/ftup.h";
#include "access/heapam.h"		/* access methods like amopenr */
#include "access/htup.h"
#include "access/itup.h"		/* for T_LOCK */
#include "parser/atoms.h"
#include "parser/parse.h"
#include "parser/parsetree.h"
#include "rules/prs2.h"
#include "rules/prs2locks.h"
#include "utils/fmgr.h"
#include "utils/log.h"
#include "utils/rel.h"		/* for Relation stuff */

#include "nodes/pg_lisp.h"
#include "nodes/relation.h"
#include "nodes/primnodes.a.h"
    
#include "catalog/catname.h"
#include "catalog/syscache.h"		/* for SearchSysCache ... */
    
#include "catalog_utils.h"

/*
 *	RelationHasLocks
 *	- returns true iff a relation has some locks on it
 */
extern RuleLock prs2GetLocksFromTuple();

RuleLock
RelationGetRelationLocks ( relation )
     Relation relation;
{
    HeapTuple relationTuple;
    RuleLock relationLocks;

    relationTuple = SearchSysCacheTuple(RELNAME,
		       &(RelationGetRelationTupleForm(relation)->relname),
		       (char *) NULL,
		       (char *) NULL,
		       (char *) NULL);

    relationLocks = prs2GetLocksFromTuple( relationTuple,
					   InvalidBuffer,
					   (TupleDescriptor) NULL );
    return(relationLocks);
}

bool
RelationHasLocks ( relation )
     Relation relation;
{
    RuleLock relationLock = RelationGetRelationLocks (relation);

    if ( relationLock == NULL )
      return (false );
    else
      return ( true );
    
}

/* 
 * RuleIdGetRuleParsetrees
 *
 * given a rule oid, look it up and return the prs2text
 *
 */

List
RuleIdGetRuleParsetrees ( ruleoid )
     OID ruleoid;
{
    HeapTuple 		ruletuple;
    char 		*ruleaction = NULL;
    bool		action_is_null = false;
    Relation 		ruleRelation = NULL;
    TupleDescriptor	ruleTupdesc = NULL;
    List    		ruleparse = NULL;
    List		i = NULL;

    ruleRelation = amopenr ("pg_prs2rule");
    ruleTupdesc = RelationGetTupleDescriptor(ruleRelation);
    ruletuple = SearchSysCacheTuple ( RULOID,  ruleoid );
    ruleaction = amgetattr ( ruletuple, InvalidBuffer , 7 , 
			    ruleTupdesc , &action_is_null ) ;
    
    ruleaction = fmgr (F_TEXTOUT,ruleaction );
    ruleparse = (List)StringToPlan(ruleaction);
    
    if ( action_is_null ) {
	printf ("action is empty !!!\n");
	return ( LispNil );
    } else {
	foreach ( i , ruleparse ) {
	    Print_parse ( CAR(i) );
	}
    }
    fflush(stdout);

    amclose ( ruleRelation );
    return (ruleparse);
}

