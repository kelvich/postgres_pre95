/* ----------------------------------------------------------------
 *   FILE
 *	RewriteSupport.c
 *
 *   DESCRIPTION
 *
 *   NOTES
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#include "tmp/postgres.h"

RcsId("$Header$");

#include "catalog/catname.h"
#include "catalog/pg_rewrite.h"
#include "catalog/syscache.h"		/* for SearchSysCache */
#include "nodes/pg_lisp.h"		/* for LispValue ... */
#include "utils/builtins.h"		/* for textout */
#include "utils/rel.h"			/* for Relation, RelationData ... */
#include "utils/log.h"		        /* for elog */
#include "storage/buf.h"		/* for InvalidBuffer */

/* 
 * RuleIdGetActionInfo
 *
 * given a rule oid, look it up and return 
 * '(rule-event-qual (rule-parsetree_list))
 *
 */

List
RuleIdGetActionInfo ( ruleoid , instead_flag)
     OID ruleoid;
     int *instead_flag;
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
    int  instead;
    ruleRelation = amopenr (RewriteRelationName);
    ruleTupdesc = RelationGetTupleDescriptor(ruleRelation);
    ruletuple = SearchSysCacheTuple (RULOID,
				     (char *) ObjectIdGetDatum(ruleoid),
				     (char *) NULL, (char *) NULL,
				     (char *) NULL);
    if (ruletuple == NULL)
	elog(WARN, "rule %d isn't in rewrite system relation");
    ruleaction = amgetattr ( ruletuple, InvalidBuffer, Anum_pg_rewrite_action, 
			    ruleTupdesc , &action_is_null ) ;
    rule_evqual_string = amgetattr (ruletuple, InvalidBuffer, 
				    Anum_pg_rewrite_ev_qual, 
				    ruleTupdesc , &action_is_null ) ;
    *instead_flag = (int) amgetattr (ruletuple, InvalidBuffer, 
				    Anum_pg_rewrite_is_instead, 
				    ruleTupdesc , &instead_is_null ) ;
    ruleaction = textout ((struct varlena *)ruleaction );
    rule_evqual_string = textout((struct varlena *)rule_evqual_string);

    ruleparse = (List)StringToPlan(ruleaction);
    rule_evqual = (List)StringToPlan(rule_evqual_string);

    if ( action_is_null ) {
	printf ("action is empty !!!\n");
	return ( LispNil );
    } else {
	foreach ( i , ruleparse ) {
#ifdef DEBUG
/*	    Print_parse ( CAR(i) ); */
#endif
	}
    }
    amclose ( ruleRelation );
    return (lispCons(rule_evqual,ruleparse));
}

char *
OperOidGetName ( oproid )
     oid oproid;
{
    HeapTuple oprtuple = NULL;
    OperatorTupleForm opform = NULL;

    oprtuple = SearchSysCacheTuple(OPROID, (char *) ObjectIdGetDatum(oproid),
				   (char *) NULL, (char *) NULL,
				   (char *) NULL);
    if ( oprtuple ) {
	opform = (OperatorTupleForm)GETSTRUCT(oprtuple);
	return ( (char *)&(opform->oprname));
    } else {
	return ("bogus-operator");
    }
    /*NOTREACHED*/
}
