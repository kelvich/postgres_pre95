/* $Header$ */
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
    printf("\n%s\n\n",querybuf);
    pg_eval(querybuf);
    fflush(stdout);
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
static char *retrieve_rule_template =
"(\"_r%s\" retrieve (\"%s\") nil 1 (((1 replace 1 ((\"*CURRENT*\" \"%s\" %d 0 nil nil)(\"*NEW*\" \"%s\" %d 0 nil nil) \"rtable\" ) \"tlist\" \"qual\" )))(1.0 . 0.0 ))";

List
FormViewRetrieveRule ( view_name,  view_tlist, view_rt, view_qual )
     Name view_name;
     List view_tlist;
     List view_rt;
     List view_qual;
{
    static char retrieve_rule_string[8192];
    List retrieve_rule 		= NULL;
    oid view_reloid 		= (oid)33376;
    List retrieve_rule_rtable	;
    List retrieve_rule_tlist	;
    List retrieve_rule_qual	;

    sprintf ( retrieve_rule_string ,
	     retrieve_rule_template,
	     view_name,
	     view_name,
	     view_name,
	     view_reloid,
	     view_name,
	     view_reloid
	     );

    retrieve_rule = (List)StringToPlan ( retrieve_rule_string );

    retrieve_rule_rtable = my_find ( "rtable", retrieve_rule );
    CAR(retrieve_rule_rtable) = view_rt;

    retrieve_rule_tlist = my_find ( "tlist", retrieve_rule );
    CAR(retrieve_rule_tlist) = view_tlist;

    retrieve_rule_qual = my_find ( "qual", retrieve_rule );
    CAR(retrieve_rule_qual) = view_qual;

    return (retrieve_rule);
    
}


static void
DefineViewRules ( view_name,  view_tlist, view_rt, view_qual )
     Name view_name;
     List view_tlist;
     List view_rt;
     List view_qual;
{
    List retrieve_rule		= NULL;
#ifdef NOTYET
    List replace_rule		= NULL;
    List append_rule		= NULL;
    List delete_rule		= NULL;
#endif

    OffsetVarNodes ( view_tlist, 2 );
    OffsetVarNodes ( view_qual, 2 );

    retrieve_rule = 
      FormViewRetrieveRule ( view_name, view_tlist , view_rt, view_qual );

#ifdef NOTYET
    
    replace_rule =
      FormViewReplaceRule ( view_name, view_tlist, view_rt, view_qual);
    append_rule = 
      FormViewAppendRule ( view_name, view_tlist, view_rt, view_qual);
    delete_rule = 
      FormViewDeleteRule ( view_name, view_tlist, view_rt, view_qual);

#endif

    DefineQueryRewrite ( retrieve_rule );
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

DefineView ( view_name, view_parse )
     Name view_name;
     List view_parse;
{
    List view_tlist 	= parse_targetlist( view_parse );
    List view_rt 	= root_rangetable ( parse_root ( view_parse ));
    List view_qual 	= parse_qualification( view_parse );

    if ( getreldesc (view_name) == NULL ) 
      DefineVirtualRelation ( view_name , view_tlist );

    DefineViewRules ( view_name, view_tlist, view_rt, view_qual );

}

