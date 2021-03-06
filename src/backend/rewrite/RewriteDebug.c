/* ----------------------------------------------------------------
 *   FILE
 *	RewriteDebug.c
 *	
 *   NOTES
 *
 *   IDENTIFICATION
 * 	$Header$
 * ----------------------------------------------------------------
 */

#include <stdio.h>
#include "parser/parsetree.h"		/* parsetree manipulation routines */
#include "rules/prs2.h"
#include "nodes/primnodes.h"		/* Var, Const ... */
#include "nodes/primnodes.a.h"
#include "catalog/syscache.h"
#include "parse.h"
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
}
/*
 * expressions can consist of varnodes, constnodes, opnodes, 
 * or lists of the above
 */

Print_expr ( expr )
     List expr;
{
    List i = NULL;

    if (!expr) {
      printf("nil ");
      return;
    }

    switch ( NodeType(expr)) {
      case classTag(LispList):
	printf ( "( " );
	foreach ( i , expr ) {
	    Print_expr(CAR(i));
	}
	printf (" )");
	break;
      case classTag(Var): 
	lispDisplay ( get_varid((Var) expr));
	break;
      case classTag(Const):
	switch ( get_consttype((Const) expr)) {
	  case 19: /* char 16 */
	    printf("%.*s", NAMEDATALEN, get_constvalue((Const) expr));
	    break;
	  case 25: /* text */
	    printf("%s",get_constvalue((Const) expr));
	    break;
	  case 26: /* oid */
	  case 23: /* int4 */
	    printf("%d",get_constvalue((Const) expr));
	    break;
	}
	break;
      case classTag(Oper):
	printf(" %.*s ", NAMEDATALEN, 
		OperOidGetName ( (ObjectId)get_opno((Oper) expr)));
	break;
      case classTag(Func):
	lispDisplay(expr);
	break;
      case classTag(Result):
	printf("result :");
	Print_expr(get_resconstantqual((Result) expr));
	break;
      case classTag(Append):
	printf("append :");
	Print_expr(get_unionplans((Append) expr));
	printf("\n");
	break;
      case classTag(SeqScan):
	printf("seqscan : qual");
	Print_expr(get_qpqual((Plan) expr));
	printf("\n");
	break;
      case classTag(NestLoop):
	printf("nestloop : qual");
	Print_expr(get_qpqual((Plan) expr));
	printf("\n");
	printf("nestloop : righttree ");
	Print_expr(get_righttree((Plan) expr));
	printf("\n");
	printf("nestloop : lefttree ");
	Print_expr(get_lefttree((Plan) expr));
	printf("\n");
	break;
      default:
	lispDisplay(expr);
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
	printf("%.*s = ", NAMEDATALEN, get_resname((Resdom) resdom));
	Print_expr ( expr );
	if (CDR(i) != NULL) 
	  printf(", ");
    }
    printf(" )\n");
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
	    printf("%.*s in %.*s",
		   NAMEDATALEN,
		   CString(rt_refname(rt_entry)),
		   NAMEDATALEN,
		   CString(rt_relname(rt_entry)));
	} else {
	    foreach ( j , rt_refname(rt_entry)) {
		printf("%s",
		       CString(CAR(j)));
	    if (CDR(j) != NULL) 
	      printf(", ");
	    }
	    printf(" in %.*s ", NAMEDATALEN, CString(rt_relname(rt_entry)));
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
	    printf(" into %.*s ", NAMEDATALEN, result_reln_name);
	} else {
	    printf(" ");
	}
	break;
      case APPEND:
      case DELETE:
      case REPLACE:
	printf(" %.*s\n", NAMEDATALEN, result_reln_name );
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

