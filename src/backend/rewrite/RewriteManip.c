/*
 * $Header$
 */


#include "nodes/relation.h"
#include "nodes/primnodes.a.h"
#include "parser/parsetree.h"
#include "parser/parse.h"

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

/*
 * AddQualifications
 * - takes a parsetree, and a new qualification
 *   and adds the new qualification onto the parsetree
 *   if the new qual is not null.
 *   
 *   if there is an existing qual, it ands it with the new qual
 *   otherwise, just stick the new qual in
 *
 * MODIFIES: parsetree
 */

void
AddQualifications ( rule_parsetree , new_qual , rule_rtlength)
     List rule_parsetree;
     List new_qual;
     int rule_rtlength;
{
    List copied_qual = NULL ;
    extern List copy_seq_tree();

    if ( new_qual  == NULL )
      return;

    copied_qual = copy_seq_tree ( new_qual );

    OffsetVarNodes ( copied_qual , rule_rtlength );
    if ( parse_qualification(rule_parsetree) == NULL )
      parse_qualification(rule_parsetree) = copied_qual;
    else
      parse_qualification ( rule_parsetree ) =
	lispCons ( lispInteger(AND),  
		  lispCons ( parse_qualification( rule_parsetree),
			    lispCons ( copied_qual, LispNil )));    
}

/*
 * FixRangeTable
 * - remove the first two entries 
 *   ( which always correspond to *current* and *new*
 *   and stick on the user rangetable entries after that
 */
void
FixRangeTable ( rule_root , user_rt )
     List rule_root;
     List user_rt;
{
    List rule_rt = root_rangetable(rule_root);

    root_rangetable(rule_root) = CDR(CDR(rule_rt));

    CDR(last(rule_rt)) = user_rt;
}

/*
 * FindMatchingNew
 * - given a parsetree, and an attribute number,
 *   find the RHS of a targetlist expression
 *   that matches the attribute we are looking for
 *   returns : matched RHS, or NULL if nothing is found
 */
static List 
FindMatchingNew ( user_tlist , attno )
     List user_tlist;
     int attno;
{
    List i = NULL;
    
    foreach ( i , user_tlist ) {
	List one_entry = CAR(i);
	List entry_LHS  = CAR(one_entry);
	
	Assert(IsA(entry_LHS,Resdom));
	if ( get_resno(entry_LHS) == attno ) {
	    return ( CDR(one_entry));
	}
    }
    return ( NULL ); /* could not find a matching RHS */
}

/*
 * MutateVarNodes
 *	- change varnodes in rule parsetree according to following :
 *	(a) varnos get offset by magnitude "offset"
 *	(b) (varno = 1) => *CURRENT* , set varno to "trigger_varno"
 *	(c) (varno = 2) => *NEW*, replace varnode with matching RHS
 *	    of user_tlist, or *CURRENT* if no such match occurs
 */

void
HandleVarNodes ( parse_subtree , user_parsetree , trigger_varno, offset )
     List parse_subtree;
     List user_parsetree;
     int trigger_varno;
     int offset;
{
    List i = NULL;

    foreach ( i , parse_subtree ) {
	Node thisnode = (Node)CAR(i);
	if ( thisnode && IsA(thisnode,Var)) {
	    List matched_new = NULL;
	    switch ( get_varno(thisnode)) {
	      case 2:
		/* *NEW* 
		 *
		 * should take the right side out of the corresponding
		 * targetlist entry from the user query
		 * and stick it here
		 */
		matched_new = 
		  FindMatchingNew ( parse_targetlist(user_parsetree) ,
				   get_varattno ( thisnode ));

		if ( matched_new ) {
		    CAR(i) = CAR(matched_new);
		    CDR(i) = CDR(matched_new);
		    break;
		}
		/* if no match, then fall thru to current */
	      case 1:
		/* *CURRENT* */
		set_varno ( thisnode, trigger_varno );
		CAR ( get_varid (thisnode) ) =
		  lispInteger ( trigger_varno );
		/* XXX - should fix the varattno too !!! */
		break;
	      default:
		set_varno ( thisnode, get_varno(thisnode) + offset );
		CAR( get_varid ( thisnode ) ) =
		  lispInteger (get_varno(thisnode) );

	    } /* end switch */
	} /* is a Var */
	if ( thisnode && thisnode->type == PGLISP_DTPR )
	  HandleVarNodes( (List)thisnode, user_parsetree, 
			  trigger_varno,offset);
    }
}


/*
 * take a parsetree and stick on the qual
 * we assume that the varnos in the qual have
 * already been appropriately munged
 */
AddEventQualifications ( parsetree, qual )
     List parsetree;
     List qual;
{


    if (null(qual)) {
	/*
	 * a null qual is always true
	 * Do nothing...
	 */
	return;
    }

    if ( parse_qualification(parsetree) == NULL )
      parse_qualification(parsetree) = qual;
    else
      parse_qualification ( parsetree ) =
	lispCons ( lispInteger(AND),  
		  lispCons ( parse_qualification( parsetree),
			    lispCons ( qual, LispNil )));    

    
}


/*
 * take a parsetree and stick on the inverse qual
 * we assume that the varnos in the qual have
 * already been appropriately munged
 */

AddNotEventQualifications ( parsetree, qual )
     List parsetree;
     List qual;
{
    
    if (null(qual)) {
	/* 
	 * A null qual is always true, so its inverse
	 * is always false.
	 * Create a dummy qual which is always false:
	 */
	Const c1;
	Const RMakeConst();

	c1 = RMakeConst();
	set_consttype(c1, (ObjectId) 16);	/* bool */
	set_constlen(c1, (Size) 1);
	set_constvalue(c1, Int8GetDatum((int8) 0));	/* false */
	set_constisnull(c1, false);
	set_constbyval(c1, true);
	qual = (List) c1;
    }
    AddEventQualifications ( parsetree, 
			     lispCons ( lispInteger(NOT), 
				       lispCons ( copy_seq_tree (qual),
						  LispNil )));

}
