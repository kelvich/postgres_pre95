/*
 * $Header$
 */

#include "nodes/pg_lisp.h"
#include "tmp/c.h"
#include "utils/log.h"
#include "nodes/nodes.h"
#include "nodes/relation.h"
#include "rules/prs2locks.h"		/* prs2 lock definitions */
#include "rules/prs2.h"			/* prs2 routine headers */
#include "nodes/primnodes.h"
#include "nodes/primnodes.a.h"
#include "parser/parsetree.h"
#include "parser/parse.h"
#include "utils/lsyscache.h"
#include "./RewriteHandler.h"
#include "./RewriteSupport.h"
#include "./locks.h"

#include "nodes/plannodes.h"
#include "nodes/plannodes.a.h"


extern List copy_seq_tree();
extern Const RMakeConst();
extern List lispCopy();


void
OffsetVarNodes ( qual_or_tlist , offset ) 
     List qual_or_tlist;
     int offset;
{
    List i = NULL;

    foreach ( i , qual_or_tlist ) {
	Node this_node = (Node)CAR(i);
	if ( this_node ) {
	    switch ( NodeType (this_node)) {
	      case classTag(LispList):
		OffsetVarNodes ( (List)this_node , offset );
		break;
	      case classTag(Var): {
		  int this_varno = (int)get_varno ( this_node );
		  int new_varno = this_varno + offset;
		  set_varno ( this_node , new_varno );
		  CAR(get_varid ( this_node)) = 
		    lispInteger ( new_varno );
		  break;
	      }
	      default:
		/* ignore the others */
		break;
	    } /* end switch on type */
	} /* if not null */
    } /* foreach element in subtree */
}
void
ChangeVarNodes ( qual_or_tlist , old_varno, new_varno)
     List qual_or_tlist;
     int old_varno, new_varno;
{
    List i = NULL;

    foreach ( i , qual_or_tlist ) {
	Node this_node = (Node)CAR(i);
	if ( this_node ) {
	    switch ( NodeType (this_node)) {
	      case classTag(LispList):
		  ChangeVarNodes ( (List)this_node , old_varno, new_varno);
		break;
	      case classTag(Var): {
		  int this_varno = (int)get_varno ( this_node );
		  if (this_varno == old_varno) {
		      set_varno (this_node, new_varno);
		      CAR(get_varid ( this_node)) = 
			  lispInteger ( new_varno );
		  }
		  break;
	      }
	      default:
		/* ignore the others */
		break;
	    } /* end switch on type */
	} /* if not null */
    } /* foreach element in subtree */
}

void AddQual(parsetree, qual)
List parsetree,qual;
{
    List copy,old;

    if (qual == NULL) return;

    copy = copy_seq_tree (qual);
    old = parse_qualification(parsetree);
    if (old == NULL)
	parse_qualification(parsetree) = copy;
    else 
	parse_qualification(parsetree) = 
	    lispCons(lispInteger(AND),
		     lispCons(parse_qualification(parsetree),
			      lispCons(copy,LispNil)));
}
void AddNotQual(parsetree, qual)
List parsetree,qual;
{
    List copy,old;

    if (qual == NULL) return;

    copy = lispCons(lispInteger(NOT),
		    lispCons(copy_seq_tree(qual), LispNil));
    AddQual(parsetree,copy);
}

static List make_null(type)
     ObjectId type;     
{
    Const c;
    
    c = RMakeConst();
    set_consttype(c, type);
    set_constlen(c, (Size) get_typlen(type));
    set_constvalue(c, PointerGetDatum(NULL));
    set_constisnull(c, true);
    set_constbyval(c, get_typbyval(type));
    return lispCons(c,LispNil);
}
void FixResdomTypes (user_tlist)
     List user_tlist;
{
    List i;
    foreach ( i , user_tlist ) {
	List one_entry = CAR(i);
	List entry_LHS  = CAR(one_entry);
	List entry_RHS = CAR(CDR(one_entry));
	
	Assert(IsA(entry_LHS,Resdom));
	if (NodeType(entry_RHS) == classTag(Var)) {
	    set_restype(entry_LHS, get_vartype(entry_RHS));
	    set_reslen (entry_LHS, get_typlen(get_vartype(entry_RHS)));
	}
    }
}

static List 
FindMatchingNew ( user_tlist , attno )
     List user_tlist;
     int attno;
{
    List i = LispNil;
    
    foreach ( i , user_tlist ) {
	List one_entry = CAR(i);
	List entry_LHS  = CAR(one_entry);
	
	Assert(IsA(entry_LHS,Resdom));
	if ( get_resno(entry_LHS) == attno ) {
	    return ( CDR(one_entry));
	}
    }
    return LispNil; /* could not find a matching RHS */
}
static List 
FindMatchingTLEntry ( user_tlist , e_attname)
     List user_tlist;
     Name e_attname;
{
    List i = LispNil;
    
    foreach ( i , user_tlist ) {
	List one_entry = CAR(i);
	List entry_LHS  = CAR(one_entry);
	Name foo;
	Resdom x;
	Assert(IsA(entry_LHS,Resdom));
	x = (Resdom) entry_LHS;	
	foo = get_resname((Resdom) entry_LHS);
	if (!strcmp(foo, e_attname))
	    return ( CDR(one_entry));
    }
    return LispNil ; /* could not find a matching RHS */
}
	
void ResolveNew(info, targetlist,tree)
RewriteInfo *info;
List targetlist;
List tree;
{
    List i,n;

    foreach ( i , tree) {
	List this_node = CAR(i);			
	if ( this_node ) {
	    switch ( NodeType (this_node)) {
	    case classTag(LispList):
		ResolveNew (info, targetlist,  (List)this_node);
		break;
	    case classTag(Var): {
		int this_varno = (int)get_varno ( this_node );
		if ((this_varno == info->new_varno) ) {
		    n = FindMatchingNew (targetlist ,
					 get_varattno(this_node));
		    if (n == LispNil) {
			if (info->event	== REPLACE) {
			    set_varno((Var) this_node, info->current_varno);
			}
			else {
			    n = (List) make_null(get_vartype(this_node));
			    CAR(i) = CAR(n);
/*			    CDR(i) = CDR(n);*/
			}
		    }
		    else {
			CAR(i) = CAR(n);
/*			CDR(i) = CDR(n);*/
		    }
		}
		break;
	    }
	    default:
		/* ignore the others */
		break;
	    } /* end switch on type */
	} /* foreach element in subtree */
    }
}

void FixNew(info, parsetree)
List parsetree;
RewriteInfo *info;
{
    
    if ((info->action == DELETE) || (info->action == RETRIEVE))	return;
    ResolveNew(info,parse_targetlist(parsetree),info->rule_action);
}
/* 
 * Handles 'on retrieve to relation.attribute
 *          do instead retrieve (attribute = expression) w/qual'
 */
void HandleRIRAttributeRule(parsetree, rt,tl, rt_index, attr_num,modified,
			    badpostquel)
     List rt;
     List parsetree, tl;
     int  rt_index, attr_num,*modified,*badpostquel;
{
    List entry, entry_LHS;
    List i,n;

    foreach ( i , parsetree) {
	List this_node = CAR(i);
	if ( this_node ) {
	    switch ( NodeType (this_node)) {
	    case classTag(LispList):
		HandleRIRAttributeRule(this_node, rt,tl, rt_index, attr_num,
				       modified);
		break;
	    case classTag(Var): {
		int this_varno = (int)get_varno ( this_node );
		List vardots = get_vardotfields((Var) this_node);
		char *name_to_look_for;
		if (this_varno == rt_index &&
		    get_varattno(this_node) == attr_num) {
		    if (vardots != LispNil) {
			name_to_look_for = CString(CAR(vardots));
		    }
		    else {
			if (get_vartype(this_node) == 32) { /* HACK */
			    n = (List) make_null(get_vartype(this_node));
			    CAR(i) = CAR(n);
/*			    CDR(i) = CDR(n);*/
			    *modified = TRUE;
			    *badpostquel = TRUE;
			    break;
			}
			else {
			    name_to_look_for = (char *)
				get_attname(CInteger(getrelid(this_varno,
							      rt)),
					    attr_num);
			}
		    }
		    n = FindMatchingTLEntry(tl,name_to_look_for);
		    if (n == NULL)
			n = (List) make_null(get_vartype(this_node));
		    CAR(i) = CAR(n);
/*		    CDR(i) = CDR(n);*/
		    if (NodeType(CAR(n)) == classTag(Var) && vardots) {
			set_vardotfields(CAR(n), lispCopy(CDR(vardots)));
		    }			
		    *modified = TRUE;
		}
	    }
		break;
	    default:
		/* ignore the others */
		break;
	    } /* end switch on type */
	} /* foreach element in subtree */
    }
}


void HandleViewRule(parsetree, rt,tl, rt_index,modified)
     List parsetree, tl,rt;
     int  rt_index,*modified;
{
    List i,n;

    foreach ( i , parsetree) {
	List this_node = CAR(i);
	if ( this_node ) {
	    switch ( NodeType (this_node)) {
	    case classTag(LispList):
		HandleViewRule((List) this_node, rt, tl, rt_index,modified);
		break;
	    case classTag(Var): {
		int this_varno = (int)get_varno ( this_node );
		if (this_varno == rt_index) {
		    Var x = (Var) this_node;

		    n = FindMatchingTLEntry
			(tl,
			 get_attname(CInteger(getrelid(this_varno,rt)),
				     get_varattno(x)));
 		    if (n == NULL)
			n = (List) make_null(get_vartype(this_node));
		    CAR(i) = CAR(n);
/*		    CDR(i) = CDR(n);*/
		    *modified = TRUE;
		}
	    }
		break;
	    default:
		/* ignore the others */
		break;
	    } /* end switch on type */
	} /* foreach element in subtree */
    }
}






