/* ----------------------------------------------------------------
 *   FILE
 *	RewriteSupport.c
 *	
 *   NOTES
 *
 *   IDENTIFICATION
 * $Header$
 * ----------------------------------------------------------------
 */

#include "rules/prs2.h"
#include "rules/prs2locks.h"

#include "utils/rel.h"			/* Relation, RelationData ... */
#include "catalog/syscache.h"		/* for SearchSysCache */
#include "utils/builtins.h"		/* for textout */

/*
 *	RelationHasLocks
 *	- returns true iff a relation has some locks on it
 */

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
    
    ruleaction = textout (ruleaction );
    ruleparse = (List)StringToPlan(ruleaction);
    
    if ( action_is_null ) {
	printf ("action is empty !!!\n");
	return ( LispNil );
    } else {
	foreach ( i , ruleparse ) {
#ifdef DEBUG
	    Print_parse ( CAR(i) );
#endif
	}
    }
    fflush(stdout);

    amclose ( ruleRelation );
    return (ruleparse);
}

char *
OperOidGetName ( oproid )
     oid oproid;
{
    HeapTuple oprtuple = NULL;
    OperatorTupleForm opform = NULL;

    oprtuple = SearchSysCacheTuple ( OPROID, oproid );
    if ( oprtuple ) {
	opform = (OperatorTupleForm)GETSTRUCT(oprtuple);
	return ( (char *)&(opform->oprname));
    } else {
	return ("bogus-operator");
    }
    /*NOTREACHED*/
}
