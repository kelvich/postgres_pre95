/* $Header$ */

#include "nodes/relation.h"
#include "nodes/relation.a.h"
#include "catalog_utils.h"
#include "nodes/primnodes.h"
#include "nodes/primnodes.a.h"

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
     char *relname;
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

/*	HandleView
 *	- takes a "viewname", "parsetree" pair and then
 *	1)	construct the "virtual" relation 
 *	2)	commit the command, so that the relation exists
 *		before the rules are defined.
 *	2)	define the "n" rules specified in the PRS2 paper
 *		over the "virtual" relation
 */

DefineView ( viewname, parsetree )
     String viewname;
     List parsetree;
{
    List tlist = parse_targetlist(parsetree);
    LispValue element;
    extern LispValue p_rtable;

    int i;

    if ( getreldesc (viewname) == NULL ) 
      DefineVirtualRelation ( viewname , tlist );

    if ( ! null ( tlist )) {
	i = 0;
	foreach ( element, tlist ) {
	    TLE 	entry = get_entry((TL)element);
	    Resdom 	res   = get_resdom(entry);
	    Var		var   = get_expr(entry);
	    int		varno = get_varno(var);
	    Name	resname = get_resname(res);
	    Name	varname;
	    Name	varattname;
	    LispValue	temp;

	    temp = (LispValue)root_rangetable(parse_root(parsetree));
	    for ( i = 1 ; i < varno ; i ++ )
	      temp = CDR(temp);

	    varname = (Name)CString(CAR(CAR(temp)));

	    varattname = attname ( varname, get_varattno(var) );
	    DefineViewRule ( viewname, resname, varname, varattname, i++ );

	}
    }	

}

DefineViewRule(viewname,viewattr,basename,baseattr,attrno)
     String viewname;
     String viewattr;
     String basename;
     String baseattr;
     AttributeNumber attrno;
{
    static char rulebuffer[1024];
    int i = 0;

    for ( i = 0 ; i < 1024 ; i++ ) {
	rulebuffer[i] = NULL;
    }

    sprintf(rulebuffer,
    "define rule _r%s%d on retrieve to %s.%s do instead retrieve %s.%s",
	    viewname,attrno,viewname,viewattr,basename,baseattr);
    sprintf(rulebuffer,
	    "define rule _a%s%d on append to %s.%s do instead",
	    viewname,attrno,viewname,viewattr,basename,baseattr);
    sprintf(rulebuffer,
	    "define rule _d%s%d on delete to %s do instead",
	    viewname,attrno,viewname,viewattr,basename,baseattr);
    sprintf(rulebuffer,
	    "define rule _i%s%d on replace to %s do instead", 
	    viewname,attrno,viewname,viewattr,basename,baseattr);
    
}


