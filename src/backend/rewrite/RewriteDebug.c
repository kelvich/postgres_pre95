/* ----------------------------------------------------------------
 *   FILE
 *	RewriteDebug.c
 *	
 *   NOTES
 *
 *   IDENTIFICATION
 * $Header$
 * ----------------------------------------------------------------
 */

#include <stdio.h>
#include "parser/parsetree.h"		/* parsetree manipulation routines */
#include "rules/prs2.h"

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

