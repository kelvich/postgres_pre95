/*     
 *      FILE
 *     	storeplan
 *     
 *      DESCRIPTION
 *     	Routines to store and retrieve plans
 *     
 */
#include "c.h"
#include "postgres.h"

RcsId("$Header$");

/*     
 *      EXPORTS
 *     		plan-save
 *     		plan-load
 *     		plan-to-string
 *     		string-to-plan
 *     		find-parameters
 *     		substitute-parameters
 */

/*
 * provide ("storeplan");
 * require ("internal");
 * #ifdef and (opus43,pg_production)
 * declare (localf (pg_out,pg_in,find_all_parameters,assoc_params))
 * #endif
 * ;
 */ 
#include "planner/internal.h"
#include "pg_lisp.h"
#include "nodes.h"

extern LispValue	string_to_plan();
extern LispValue	plan_to_string();
extern LispValue	pg_out();
/*    
 *    	plan-save
 *    	plan-load
 *    
 *    	Public disk file interface.
 *    
 */
LispValue
plan_save (plan, filename)
	LispValue plan;
	LispValue filename;
{
	with_open_file (ofile (filename),print (plan_to_string (plan),ofile));
}
LispValue
plan_load (filename)
	LispValue filename;
{
	with_open_file (ifile (filename),string_to_plan (read (ifile)));
}

/*    
 *    	string-to-plan
 *    	plan-to-string
 *    
 *    	Public string interface.
 *    
 */

/*  .. plan-load
 */
LispValue
string_to_plan (string)
	LispValue string;
{
	pg_in (read_from_string (string));
}

/*  .. plan-save
 */
LispValue
plan_to_string (plan)
LispValue plan;
{
	pg_out (plan);
}

/*    
 *    	pg-out
 *    	pg-in
 *    
 *    	Internal routines.
 */
LispValue
pg_out (tree)
	LispValue tree;
{
	/*   ok to make write expensive -- write once, read many */
	/* XXX - let form, maybe incorrect */
	/*
	 * if(member (type_of (tree),_node_types_)) {
	 * 	strcat ("#S(",vprop (tree),
	 * 		" ",
	 * 		dotimes (i (vsize (tree),s),
	 * 		s = strcat (s,pg_out (aref (tree,i)));),")");
	 *
	 * } else if (vectorip (tree)) {
	 * 	vectori_to_string (tree);
	 *
	 * } else if (consp (tree)) {
	 * 	strcat ("(",foreach (elt, trees) {
	 * 		s = strcat (s,pg_out (elt));;
	 * 		    ,")");
	 *
	 * 	} else {
	 * strcat (" ",prin1_to_string (tree));
	 */

	 (*((Node)tree)->printFunc)(tree);
}


/*
 *  Mao says:  this stuff must already exist elsewhere, or plans wouldn't
 *	       work at all.  Turn it off.
 */

#ifdef notdef

LispValue
reader (stream,subchar,arg)
	LispValue stream,subchar,arg ;
{
	declare (ignore (subchar,arg));
	/* XXX - let form, maybe incorrect */
	LispValue items = read_delimited_list ("#']",stream,1 /* XXX - true */);
	make_array (length (items),element_type,/* XXX- QUOTE fixnum,*/,initial_contents,items);
	;
}

/*  (set-dispatch-macro-character  #\# #\[ #'|#[-reader|)
 *  (set-macro-character #\] (get-macro-character #\)) nil)
 *  .. pg-in, string-to-plan
 */
/*
 *	define (pg_in,identity);
 */

/*   will read plans many times */
/*
 *  Mao says:  don't need these anymore.  Only place that _node_types_
 *	       was used was in this file, but we now have other ways of
 *	       determining whether a LispValue is a node.
 *
 * defvar (_node_types_, QUOTE append,const,existential,func,
 *			indexscan,mergesort,nestloop,oper,param,resdom,
 *			result,seqscan,sort,var,rule_lock,);
 */
/*
 * .. pg-in, string-to-plan
 */
LispValue
pg_in (tree)
	LispValue tree ;
{
	if(consp (tree)) {
		switch (nth (0,tree)) {

		case vector:
					    { /* XXX - let form, maybe incorrect */
						LispValue new_vector = apply (/* XXX - hash-quote */ vector,mapcar (/* XXX - hash-quote */ pg_in,
						    cddr (tree)));
						vsetprop (new_vector,nth (1,tree));
						new_vector;
						;
					}
					break;

				case: 
					vectori
					    { 
						apply (/* XXX - hash-quote */ vectori_byte,cdr (tree));
					}
					break;

				case: 
					list
					    { 
						mapcar (/* XXX - hash-quote */ pg_in,cdr (tree));
					}
					break;

				default: /* do nothing */
				};


			} else if (1 /* XXX - true */) {
				tree;

			} else if ( true ) ;  
			end-cond ;
		}

		/*    
 *    	find-parameters
 *    
 *    	Extracts all paramids from a plan tree.
 *    
 */
		LispValue
		    find_parameters (planplan)
		    LispValue plan ;
		{
			remove_duplicates (find_all_parameters (plan));
		}

		/*  .. find-all-parameters, find-parameters
 */
		LispValue
		    find_all_parameters (treetree)
		    LispValue tree ;
		{
			if(param_p (tree)) {
				list (get_paramid (tree));

			} else if (consp (tree)) {
				mapcan (/* XXX - hash-quote */ find_all_parameters,tree);

			} else if (vectorp (tree)) {
				/* XXX - let form, maybe incorrect */
				LispValue plan_list = LispNil;
				dotimes (i (vsize (tree),plan_list),plan_list = nconc (find_all_parameters (aref (tree,i)),
				    plan_list););
				;

			} else if ( true ) ;  
			end-cond ;
		}

		/*    
 *    	substitute-parameters
 *    
 *    	Find param nodes in a structure and change them to constants, as
 *    	specified by an list of parameter ids and values.
 *    
 *    	'plan' is a query plan (or part of a query plan) which (may) contain
 *    		param nodes.
 *    	'params' is a list of values to substitute for parameters in the form:
 *    		(fixnum consttype constvalue)		for $1 params
 *    		("attname" consttype constvalue) 	for $.attname params
 *    
 *    	Returns a new copy of the plan data structure with the parameters
 *    	substituted in.
 *    
 */
		LispValue
		    substitute_parameters (planplan,paramsparams)
		    LispValue plan,params ;
		{
			if(null (plan)) {
				nil;

			} else if (null (params)) {
				plan;

			} else if (1 /* XXX - true */) {
				/* XXX - let form, maybe incorrect */
				LispValue param_alist = mapcar (/* XXX - hash-quote */ lambda (param (),cons (nth (0,param),
				    make_const (nth (1,param),get_typlen (nth (1,param)),nth (2,param),nil))),params);
				assoc_params (plan,param_alist);
				;

			} else if ( true ) ;  
			end-cond ;
		}

		/*    
 *    	assoc-params
 *    
 *    	Substitutes the const nodes generated by param-alist into the plan.
 *    
 *    	Returns a new copy of the plan data structure with the parameters
 *    	substituted in.
 *    
 */

		/*  .. assoc-params, substitute-parameters
 */
		LispValue
		    assoc_params (planplan,param_alistparam_alist)
		    LispValue plan,param_alist ;
		{
			if(param_p (plan)) {
				or (cdr (assoc (get_paramid (plan),param_alist)),plan);

			} else if (consp (plan)) {
				mapcar (/* XXX - hash-quote */ lambda (x (),assoc_params (x,param_alist)),plan);

			} else if (vectorp (plan)) {
				LispValue vector_size = vsize (plan);
				LispValue newplan = new_vector (vector_size);
				dotimes (i (vector_size),setf (aref (newplan,i),assoc_params (aref (plan,i),param_alist)));
				vsetprop (newplan,vprop (plan));
				newplan;
				;

			} else if (1 /* XXX - true */) {
				plan;

			} else if ( true ) ;  
			end-cond ;
		}
#endif /* notdef */
