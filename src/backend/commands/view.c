/* 
 * $Header$
 */

#include "access/heapam.h"
#include "catalog/syscache.h"
#include "utils/log.h"
#include "nodes/relation.h"
#include "nodes/relation.a.h"
#include "nodes/primnodes.h"
#include "nodes/primnodes.a.h"
#include "parser/parsetree.h"

/* #include "catalog_utils.h" */ 

#include "./RewriteManip.h"

extern Name tname();

Name
attname ( relname , attnum )
     Name relname;
     int attnum;
{
    Relation relptr;
    HeapTuple atp;
    ObjectId reloid;
    Name varname;

    relptr = amopenr (relname );
    reloid = RelationGetRelationId ( relptr );
   
    atp = SearchSysCacheTuple ( ATTNUM, reloid, attnum , NULL, NULL );
    if (!HeapTupleIsValid(atp)) {
	elog(WARN, "getattnvals: no attribute tuple %d %d",
	     reloid, attnum);
	return(0);
    }
    varname = (Name)&( ((AttributeTupleForm) GETSTRUCT(atp))->attname );

    return ( varname );
}

/*	DefineVirtualRelation
 *	- takes a relation name and a targetlist
 *	and generates a string "create relation ( ... )"
 *	by taking the attributes and types from the tlist
 *	and reconstructing the attr-list for create.
 *	then calls "pg-eval" to evaluate the creation,
 */
  
DefineVirtualRelation ( relname , tlist )
     Name relname;
     List tlist;
{
    LispValue element;
    static char querybuf[1024];
    char *index = querybuf;
    int i;

    for ( i = 0 ; i < 1024 ; i++ ) {
	querybuf[i] = NULL;
    }

    sprintf(querybuf,"create %s(",relname );
    index += strlen(querybuf);
    
    if ( ! null ( tlist )) {
	foreach ( element, tlist ) {
	    TLE 	entry = get_entry((TL)element);
	    Resdom 	res   = get_resdom(entry);
	    Name	resname = get_resname(res);
	    ObjectId	restype = get_restype(res);
	    Name	restypename = tname(get_id_type(restype));

	    sprintf(index,"%s = %s,",resname,restypename);
	    index += strlen(index);
	}
	*(index-1) = ')';
	
    } else
	elog ( WARN, "attempted to define virtual relation with no attrs");
    pg_eval(querybuf);
}    

#ifdef BOGUS
void 
sprint_tlist ( addr_of_target, tlist, rt )
     String *addr_of_target;
     List tlist;
     List rt;
{
    static char temp_buffer[8192];
    char *index = 0;
    List i = NULL;

    foreach ( i , tlist ) {
	List 	this_tle	= CAR(i);
	Resdom 	this_resdom 	= NULL;
	List	this_expr 	= NULL;
	char	*this_resname 	= NULL;
	char	*this_restype 	= NULL;

	this_expr = tl_expr(tle);
	this_resdom = (Resdom)tl_resdom(tle);

	Assert ( IsA(this_resdom,Resdom));

	this_resname = get_resname(this_resdom);
	this_resexpr = expr_to_string ( );

	sprintf ( (temp_buffer + index) , "%s = %s::%s",
		  this_resname,
		  this_resexpr,
		  this_restype );

    }
}
#endif BOGUS

List
my_find ( string, list )
     char *string;
     List list;
{
    List i = NULL;
    List retval = NULL;

    for( i = list ; i != NULL; i = CDR(i) ) {
	if ( CAR(i) && IsA (CAR(i),LispList)) {
	    retval = my_find ( string,CAR(i));
	    if ( retval ) 
	      break;
	} else if ( CAR(i) && IsA(CAR(i),LispStr) && 
		    !strcmp(CString(CAR(i)),string) ) {
	    retval = i;
	    break;
	} 
    }
    return(retval);
}

List
FormViewRetrieveRule (view_name, view_parse)
Name view_name;
List view_parse;
{
    
    char rname[100];
    List p, q, target, rt;
    List lispCopy();

    /*
     * Create a parse tree that corresponds to the suitable
     * tuple level rule...
     */
    p = LispNil;
    /*
     * this is an instance-level rule... (or tuple-level
     * for the traditionalists)
     */
    p = nappend1(p, lispAtom("instance"));
    /*
     * the second item in the list is the so called 'hint' : "(rule nil)"
     */
    p = nappend1(p, lispCons(lispAtom("rule"), lispCons(LispNil, LispNil)));
    /*
     * rule name now...
     * Note that we handle the cases where the rule name is more
     * than 16 chars long... I feel really proud of my defensive
     * programming style !!!!!
     */
    sprintf(rname, "ret_%s", view_name);
    rname[15] = 0;
    p = nappend1(p, lispString(rname));
    /*
     * The next item is a big one...
     * First the 'event type' (retrieve), then the 'target' (the
     * view relation), then the 'rule qual' (no qualification in this
     * case), then the 'instead' information (yes, this is an instead
     * rule!) and finally the 'rule actions', whish is nothing else from
     * a list containing the view parse!
     */
    q = LispNil;
    q = nappend1(q, lispAtom("retrieve"));
    target = lispCons(lispString(view_name), LispNil);
    q = nappend1(q, target);
    q = nappend1(q, LispNil);
    q = nappend1(q, lispInteger(1));
    q = nappend1(q, lispCons(lispCopy(view_parse), LispNil));
    p = nappend1(p, q);
    /*
     * now our final item, the range table...
     */
    rt = lispCopy(root_rangetable(parse_root(view_parse)));
    p = nappend1(p, rt);

    return(p);

}


static void
DefineViewRules (view_name, view_parse)
Name view_name;
List view_parse;
{
    List retrieve_rule		= NULL;
    char ruleText[1000];
#ifdef NOTYET
    List replace_rule		= NULL;
    List append_rule		= NULL;
    List delete_rule		= NULL;
#endif

    retrieve_rule = 
      FormViewRetrieveRule (view_name, view_parse);

#ifdef NOTYET
    
    replace_rule =
      FormViewReplaceRule ( view_name, view_tlist, view_rt, view_qual);
    append_rule = 
      FormViewAppendRule ( view_name, view_tlist, view_rt, view_qual);
    delete_rule = 
      FormViewDeleteRule ( view_name, view_tlist, view_rt, view_qual);

#endif

    sprintf(ruleText, "retrieve rule for view %s", view_name);
    prs2DefineTupleRule(retrieve_rule, ruleText);

#ifdef NOTYET
    DefineQueryRewrite ( replace_rule );
    DefineQueryRewrite ( append_rule );
    DefineQueryRewrite ( delete_rule );
#endif

}     
       
/*	HandleView
 *	- takes a "viewname", "parsetree" pair and then
 *	1)	construct the "virtual" relation 
 *	2)	commit the command, so that the relation exists
 *		before the rules are defined.
 *	2)	define the "n" rules specified in the PRS2 paper
 *		over the "virtual" relation
 */

DefineView(view_name, view_parse)
Name view_name;
List view_parse;
{

    List view_tlist;

    view_tlist = parse_targetlist( view_parse );

    /*
     * Create the "view" (if it does not already exist).
     * XXX: should we complain if it alrready exists ?
     */
    if ( getreldesc (view_name) == NULL ) {
	DefineVirtualRelation (view_name ,view_tlist);
	/*
	 * The relation we have just created is not visible
	 * to any other commands running with the same transaction &
	 * command id.
	 * So, increment the command id counter (but do NOT pfree any
	 * memory!!!!)
	 */
	CommandCounterIncrement();
    }

    /*
     * The range table of 'view_parse' does not contain entries
     * for the "CURRENT" and "NEW" relations.
     * So... add them!
     * NOTE: we make the update in place! After this call 'view_parse' 
     * will never be what it used to be...
     */
    UpdateRangeTableOfViewParse(view_name, view_parse);
    DefineViewRules(view_name, view_parse);


}

/*---------------------------------------------------------------
 * UpdateRangeTableOfViewParse
 *
 * Update the range table of the given parsetree.
 * This update consists of adding two new entries IN THE BEGINNING
 * of the range table (otherwise the rule system will die a slow,
 * horrible and painful death, and we do not want that now, do we?)
 * one for the CURRENT relation and one for the NEW one (both of
 * them refer in fact to the "view" relation).
 *
 * Of course we must also increase the 'varnos' of all the Var nodes
 * by 2...
 *
 * NOTE: these are destructive changes. It would be difficult to
 * make a complete copy of the parse tree and make the changes
 * in the copy. 'lispCopy' is not enough because it onyl does a high
 * level copy (i.e. the Var nodes remain the same, so the varno offset
 * will update both parsetrees) and I am not confident that
 * 'copyfuncs.c' are bug free...
 *---------------------------------------------------------------
 */
void
UpdateRangeTableOfViewParse(view_name, view_parse)
Name view_name;
List view_parse;
{
    List old_rt;
    List new_rt;
    List root;
    List rt_entry1, rt_entry2;
    List MakeRangeTableEntry();

    /*
     * first offset all var nodes by 2
     */
    OffsetVarNodes(view_parse, 2);

    /*
     * find the old range table...
     */
    root = parse_root(view_parse);
    old_rt = root_rangetable(root);

    /*
     * create the 2 new range table entries and form the new
     * range table...
     * CURRENT first, then NEW....
     */
    rt_entry1 = MakeRangeTableEntry(view_name, LispNil, "*CURRENT*");
    rt_entry2 = MakeRangeTableEntry(view_name, LispNil, "*NEW*");
    new_rt = lispCons(rt_entry2, old_rt);
    new_rt = lispCons(rt_entry1, new_rt);

    /*
     * Now the tricky part....
     * Update the range table in place... Be careful here, or
     * hell breaks loooooooooooooOOOOOOOOOOOOOOOOOOSE!
     */
    root_rangetable(root) = new_rt;
}
