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
#include "nodes/primnodes.h"		/* Var, Const ... */
#include "nodes/primnodes.a.h"
#include "catalog/syscache.h"
#include "parser/parse.h"
#include "./RewriteSupport.h"

/* to print out plans */

#include "nodes/plannodes.h"
#include "nodes/plannodes.a.h"

#ifdef NOTYET
char *
VarOrUnionGetDesc ( varnode , rangetable )
     Var varnode;
     List rangetable;
{
    int rt_index	= 0;
    int attnum		= 0;
    List this_rt_entry 	= NULL;

    if ( IsA(varnode,Var) ) {
	rt_index = get_varno(varnode);
	attnum = get_varattno(varnode);
	
	this_rt_entry = nth(rt_index,rangetable);
    } 
    
}
#endif

Print_quals ( quals )
     List quals;
{
    printf("where ");
    Print_expr(quals);
    printf("\n");
#ifdef OLDCODE
    printf("\nQualifications = \n");
    lispDisplay(quals,0);
    fflush(stdout);
#endif
}
/*
 * expressions can consist of varnodes, constnodes, opnodes, 
 * or lists of the above
 */

Print_expr ( expr )
     List expr;
{
    List i = NULL;

    if ( expr == NULL )
      printf("nil\n");

    switch ( NodeType(expr)) {
      case classTag(LispList):
	printf ( "( " );
	foreach ( i , expr ) {
	    Print_expr(CAR(i));
	}
	printf (" )");
	break;
      case classTag(Var): 
	lispDisplay ( get_varid(expr), 0 );
	break;
      case classTag(Const):
	switch ( get_consttype(expr)) {
	  case 19: /* char 16 */
	  case 25: /* text */
	    printf("%s",get_constvalue(expr));
	    break;
	  case 26: /* oid */
	  case 23: /* int4 */
	    printf("%d",get_constvalue(expr));
	    break;
	}
	break;
      case classTag(Oper):
	printf(" %s ",OperOidGetName ( (ObjectId)get_opno(expr)));
	break;
      case classTag(Func):
	lispDisplay(expr, 0);
	break;
      case classTag(Result):
	printf("result :");
	Print_expr(get_resconstantqual(expr));
	break;
      case classTag(Append):
	printf("append :");
	Print_expr(get_unionplans(expr));
	printf("\n");
	break;
      case classTag(SeqScan):
	printf("seqscan : qual");
	Print_expr(get_qpqual(expr));
	printf("\n");
	break;
      case classTag(NestLoop):
	printf("nestloop : qual");
	Print_expr(get_qpqual(expr));
	printf("\n");
	printf("nestloop : righttree ");
	Print_expr(get_righttree(expr));
	printf("\n");
	printf("nestloop : lefttree ");
	Print_expr(get_lefttree(expr));
	printf("\n");
	break;
      default:
	lispDisplay(expr, 0 );
	break;
    }
}

Print_targetlist ( tlist )
     List tlist;
{
    List i = NULL;

    printf(" ( ");
    foreach ( i , tlist ) {
	List entry = CAR(i);
	List resdom = tl_resdom(entry);
	List expr = tl_expr(entry);

	Assert(IsA(resdom,Resdom));
	printf("%s = ", get_resname(resdom));
	Print_expr ( expr );
	if (CDR(i) != NULL) 
	  printf(", ");
    }
    printf(" )\n");
    fflush(stdout);
}
#define rt_refname(rt_entry) CAR(rt_entry)
#undef rt_relname(rt_entry)
#define rt_relname(rt_entry) CADR(rt_entry)

Print_rangetable ( rtable )
     List rtable;
{
    List i = NULL;
    List j = NULL;

    printf("from ");

    foreach (i,rtable) {
	List rt_entry = CAR(i);
	if ( IsA (rt_refname(rt_entry),LispStr) ) {
	    printf("%s in %s",
		   CString(rt_refname(rt_entry)),
		   CString(rt_relname(rt_entry)));
	} else {
	    foreach ( j , rt_refname(rt_entry)) {
		printf("%s",
		       CString(CAR(j)));
	    if (CDR(j) != NULL) 
	      printf(", ");
	    }
	    printf(" in %s ",CString(rt_relname(rt_entry)));
	}
	if (CDR(i) != NULL) 
	  printf(", ");
    }
    printf("\n");
    fflush(stdout);
}

Print_parse ( parsetree )
     List parsetree;
{
    List quals = parse_qualification(parsetree);
    List tlist = parse_targetlist(parsetree);
    List rtable = root_rangetable (parse_root (parsetree));
    List result_reln = root_result_relation(parse_root(parsetree));
    char *result_reln_name = NULL;

    if ( result_reln ) {
	if (IsA(result_reln,LispInt))
	  result_reln_name = CString(CADR(nth(CInteger(result_reln)-1,rtable)));
    }
    lispDisplay(CADR(parse_root(parsetree)));
    switch (root_command_type(parse_root(parsetree))) {
      case RETRIEVE:
	if (result_reln) {
	    printf(" into %s ",result_reln_name);
	} else {
	    printf(" ");
	}
	break;
      case APPEND:
      case DELETE:
      case REPLACE:
	printf(" %s\n",result_reln_name );
	break;
    }
    Print_targetlist(tlist);
    Print_rangetable(rtable);
    if ( quals ) 
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

