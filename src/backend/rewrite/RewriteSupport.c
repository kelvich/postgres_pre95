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
 * RuleIdGetRuleParsetrees
 *
 * given a rule oid, look it up and return the prs2text
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
    List		i = NULL;
    bool		is_instead = false;

    ruleRelation = amopenr ("pg_rewrite");
    ruleTupdesc = RelationGetTupleDescriptor(ruleRelation);
    ruletuple = SearchSysCacheTuple ( RULOID,  ruleoid );

    ruleaction = amgetattr ( ruletuple, InvalidBuffer , Anum_pg_rewrite_action, 
			    ruleTupdesc , &action_is_null ) ;

    is_instead = (bool)amgetattr ( ruletuple, InvalidBuffer , 
				  Anum_pg_rewrite_is_instead, ruleTupdesc,
				  &instead_is_null );
    
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
    return (lispCons(is_instead,ruleparse));
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
