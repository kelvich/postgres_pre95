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

#include "catalog/pg_rewrite.h"

#include "utils/rel.h"			/* Relation, RelationData ... */
#include "catalog/syscache.h"		/* for SearchSysCache */
#include "utils/builtins.h"		/* for textout */

/* 
 * RuleIdGetActionInfo
 *
 * given a rule oid, look it up and return 
 * '(rule-event-qual rule-parsetree)
 *
 */

List
RuleIdGetActionInfo ( ruleoid )
     OID ruleoid;
{
    HeapTuple 		ruletuple;
    char 		*ruleaction = NULL;
    bool		action_is_null = false;
    bool		instead_is_null = false;
    Relation 		ruleRelation = NULL;
    TupleDescriptor	ruleTupdesc = NULL;
    List    		ruleparse = NULL;
    char		*rule_evqual_string = NULL;
    List		rule_evqual = NULL;
    List		i = NULL;

    ruleRelation = amopenr ("pg_rewrite");
    ruleTupdesc = RelationGetTupleDescriptor(ruleRelation);
    ruletuple = SearchSysCacheTuple ( RULOID,  ruleoid );

    ruleaction = amgetattr ( ruletuple, InvalidBuffer, Anum_pg_rewrite_action, 
			    ruleTupdesc , &action_is_null ) ;
    rule_evqual_string = amgetattr (ruletuple, InvalidBuffer, 
				    Anum_pg_rewrite_ev_qual, 
				    ruleTupdesc , &action_is_null ) ;

    ruleaction = textout (ruleaction );
    rule_evqual_string = textout(rule_evqual_string);

    ruleparse = (List)StringToPlan(ruleaction);
    rule_evqual = (List)StringToPlan(rule_evqual_string);

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
    return (lispCons(rule_evqual,ruleparse));
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
