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

Print_quals ( quals )
     List quals;
{
    printf("\nQualifications = \n");
    lispDisplay(quals,0);
    fflush(stdout);
}
Print_targetlist ( tlist )
     List tlist;
{
    printf("\nTargetlist = \n");
    lispDisplay(tlist,0);
    fflush(stdout);
}
Print_rangetable ( rtable )
     List rtable;
{
    printf("\nRangetable = \n");
    lispDisplay(rtable,0);
    fflush(stdout);
}

Print_parse ( parsetree )
     List parsetree;
{
    List quals = parse_qualification(parsetree);
    List tlist = parse_targetlist(parsetree);
    List rtable = root_rangetable (parse_root (parsetree));
    
    Print_rangetable(rtable);
    Print_targetlist(tlist);
    Print_quals(quals);
    fflush(stdout);
}

void
PrintRuleLock ( rlock )
     Prs2OneLock rlock;
{
    printf("#S(RuleLock ");
    printf(":rulid %ld ",rlock->ruleId);
    printf(":locktype %c ", rlock->lockType );
    printf(":attnum %d )", rlock->attributeNumber );
    fflush(stdout);
}

void
PrintRuleLockList ( rlist )
     List rlist;
{
    Prs2OneLock temp = NULL;
    List j = NULL;
    foreach ( j , rlist ) {
	temp = (Prs2OneLock)CAR(j);
	PrintRuleLock ( temp );
    }
    printf("\n");
}

