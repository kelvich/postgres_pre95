
/*     
 *      FILE
 *     	pppp
 *     
 *      DESCRIPTION
 *     	POSTGRES plan pretty printer
 *     
 */
static char *rcsid =
"$Header$";

/*     
 *      EXPORTS
 *     		print_parse
 *     		print_plan
 *
 *  provide ("pppp");
 */

#include "internal.h";

/*  #ifdef opus43
 *  declare (special (_newline_))
 */ #endif

/* #ifdef (opus43 && pg_production)
 * declare (localf (print_root, print_subplan, print_rtentries,
 *		 print_tlist, print_tlistentry, print_allqual,
 *		 print_qual, print_var, print_clause, print_clauses,
 *		 print_const, print_param,indent)
 *      )
 * #endif
 */
LispValue
print_parse (parse)
LispValue parse ;
{
	_query_range_table_ = root_rangetable (parse_root (parse));
	print_root (parse_root (parse));
	mesg ("TargetList:\n");
	print_tlist (parse_targetlist (parse),0);
	mesg ("Qualification:\n");
	print_qual (cnfify (parse_qualification (parse)),0);
}

/*  .. print_parse
 */
LispValue
print_root (root)
LispValue root ;
{
	mesg ("NumLevels: ", root_levels (root), "\n",
	      "CommandType: ", root_command_type (root), "\n",
	      "ResultRelation: ", root_result_relation (root), "\n",
	      "RangeTable:\n");
	print_rtentries (root_rangetable (root), 0);
	mesg ("Priority: ", root_priority (root), "\n",
	      "RuleInfo: ", root_ruleinfo (root), "\n");
}

/*  .. print_plan, print_subplan
 */
LispValue
print_plan (plan, levelnum)
LispValue plan, levelnum;
{
int i;
LispValue saved_rt;
	if(append_p (plan)) {
		mesg ("Append", levelnum, "\n\n",
		      "Inheritance Relation : ", get_inhrelid (plan), "\n");
		/* XXX - let form, maybe incorrect */
		saved_rt = copy_seq_tree (_query_range_table_);
		for (i = 0; i < (length (get_plans (plan)); i++){
		  mesg ("Appended Plan", i, "\n");
		  _query_range_table_ = fix_rangetable (_query_range_table_,
							get_inhrelid (plan),
							nth (i, get_rtentries (plan))),
		  print_plan (nth (i, get_plans (plan)),
		    0);
		    };
		_query_range_table_ = saved_rt;
		
		mesg ("Inheritance Rangetable :\n");
		print_rtentries (get_rtentries (plan), 0);

	} else if (existential_p (plan)) {
		mesg ("Existential", levelnum, "\n");
		if /*when */ ( get_lefttree (plan)) {
			mesg ("\nExistential Plan", levelnum, "\n");
			print_plan (get_lefttree (plan), 0);
		};
		if /*when */ ( get_righttree (plan)) {
			mesg ("\nQuery Plan", levelnum, "\n");
			print_plan (get_righttree (plan), 0);
		};

	} else if (result_p (plan)) {
		mesg ("Result", levelnum, "\n\n");
		mesg ("TargetList :\n");
		print_tlist (get_qptargetlist (plan), 0);
		mesg ("RelLevelQual :\n");
		print_qual (get_resrellevelqual (plan), 0);
		mesg ("ConstantQual :\n");
		print_qual (get_resconstantqual (plan), 0);
		if /*when */ ( get_lefttree (plan)) {
			mesg ("\nSubplan", levelnum, "\n\n");
			print_subplan (get_lefttree (plan), 0);
		};
		if /*when */ ( get_righttree (plan)) {
			print_plan (get_righttree (plan), (levelnum + 1));
		};

	} else if (1 /* XXX - true */) {
		mesg ("Subplan", levelnum, "\n\n");
		print_subplan (plan, 0);

	} else if ( true ) ;  /* what is this all about jeff? this should be an error cond*/
	end-cond ;
}

/*  .. print_plan, print_subplan
 */
LispValue
print_subplan (subplan, levels)
LispValue subplan, levels ;
{
	indent (levels);
	mesg ("        PathType : ",type_of (subplan), "\n");
	indent (levels);
	mesg ("        Cost : ", get_state (subplan), "\n");
	if /*when */ ( scan_p (subplan)) {
		indent (levels);
		mesg ("        Relid : ");
		if (integerp (get_scanrelid (subplan)) &&
		    (_TEMP_RELATION_ID_ != get_scanrelid (subplan)))
		  {
		  mesg (getrelname (get_scanrelid (subplan),_query_range_table_));

		} else {
		  mesg (get_scanrelid (subplan));
		};
		mesg("\n");
	};
	if (temp_p (subplan)) {
		indent (levels);
		mesg ("        TempID : ", get_tempid (subplan), "\n");
		indent (levels);
		mesg ("        KeyCount : ", get_keycount (subplan), "\n");
		indent (levels);
		mesg ("        TempList :\n");
		print_tlist (get_qptargetlist (subplan), (levels + 1)); 

	} else if (1 /* XXX - true */) {
		indent (levels);
		mesg ("        TargetList :\n");
		print_tlist (get_qptargetlist (subplan), (levels + 1));
		indent (levels);
		mesg ("        Qual :\n");
		print_qual (get_qpqual (subplan), (levels + 1));

	} else if ( true ) ;  
	end-cond ;
	if /*when */ ( mergesort_p (subplan)) {
		indent (levels);
		mesg ("        MergeSortOp : ", get_mergesortop (subplan), "\n");
		indent (levels);
		mesg ("        MergeClauses :\n");
		print_qual (get_mergehashclauses (subplan), (levels + 1));
	};
	if /*when */ ( indexscan_p (subplan)) {
		indent (levels);
		mesg ("        IndexID : ", get_indxid (subplan), "\n");
		indent (levels);
		mesg ("        IndexQual :\n");
		print_allqual (get_indxqual (subplan), (levels + 1));
		mesg ("\n");
	};
	if (join_p (subplan)) {
		indent (levels);
		mesg ("        OuterPath :\n");
		print_subplan (get_lefttree (subplan), (levels + 1));
		indent (levels);
		mesg ("        InnerPath :\n");
		print_subplan (get_righttree (subplan), (levels + 1));

	} else if (get_lefttree (subplan)) {
		mesg ("\n");
		if (result_p (get_lefttree (subplan))) {
			print_plan (get_lefttree (subplan), 1);

		} else if (1 /* XXX - true */) {
			print_subplan (get_lefttree (subplan), levels + 1);

		} else if ( true ) ;  
		end-cond ;

	} else if ( true ) ;  
	end-cond ;
}

/*  .. print_plan, print_root
 */
LispValue
print_rtentries (rt, levels)
LispValue rt, levels ;
{
	foreach (rtentry, rt) {
		indent (levels + 1);
		mesg ("(", rt_relname (rtentry),
		      " ", rt_relid (rtentry),
		      " :time ", rt_time (rtentry),
		      " :flags ", rt_flags (rtentry),
		      " :rulelocks ");
		if (integerp (rt_rulelocks (rtentry))) {
			mesg ("\n", " ");
			indent (levels + 1);
			print_rule_lock_intermediate (rt_rulelocks (rtentry));
			indent (levels + 1);

		} else if (1 /* XXX - true */) {
			mesg (rt_rulelocks (rtentry));

		} else if ( true ) ;  
		end-cond ;
		mesg (")\n");

	      };
      }

	/*  .. print_parse, print_plan, print_subplan
 */
LispValue
print_tlist (tlist, levels)
     LispValue tlist,levels ;
{
  foreach (entry, tlist) {
    indent (levels + 1);
    print_tlistentry (entry);
  }
}

/*  .. print_tlist 
*/
LispValue
print_tlistentry (tlistentrytlistentry)
     LispValue tlistentry ;
{
   LispValue resdom, exp;

   resdom = tl_resdom (tlistentry);
   expr = tl_expr (tlistentry);
   mesg ("(", get_resno (resdom)," ");

   if /*when */ (get_reskey (resdom)) {                    /* not equal to 0 */
      mesg ("(", get_reskey (resdom), " ", get_reskeyop (resdom), ") ");
   };
   if /*when */ ( get_resname (resdom)) {
      mesg (get_resname (resdom), " ");
   };
   print_clause (expr, 0);
   mesg (")\n");

}

/*  .. print_subplan 
*/
LispValue
print_allqual (quals, levels)
     LispValue quals, levels ;
{
   foreach (qual, quals) {
      print_qual (qual,levels);
      mesg ("\n");
      
   };
}
   
/*  .. print_allqual, print_parse, print_plan, print_subplan
 */
LispValue
print_qual (qual, levels)
     LispValue qual,levels ;
{
   foreach (clause, qual)
      {
      mesg ("        ");
      print_clause (clause, levels);
      mesg ("\n");

   };
}
   
/*  .. print_clause
 */
LispValue
print_var (var)
     LispValue var ;
{
   mesg ("(");
   if(_query_range_table_ &&
      integerp (get_varno (var)) &&
      (_TEMP_RELATION_ID_ != get_varno (var)))
      {
      mesg (getrelname (get_varno (var),_query_range_table_), " ",
	    get_attname (getrelid (get_varno (var),_query_range_table_),
			 get_varattno (var)));

   } else if (1 /* XXX - true */) {
      mesg (get_varno (var), " ", get_varattno (var));

   } else if ( true ) ;  
   end-cond ;
   
   foreach (dot, get_vardotfields (var)) {
      mesg (" ", dot);
   };
   if /*when */ ( get_vararrayindex (var)) {
      mesg ("(", get_vararrayindex (var), ")");
   };
   mesg (")");
}

/*  .. print_clause, print_clauses, print_qual, print_tlistentry
 */
LispValue
print_clause (clause, levels)
     LispValue clause, levels ;
{
   if /*when */ (clause) {
      indent (levels);
      if(var_p (clause)) {
	 print_var (clause);
	 
      } else if (const_p (clause)) {
	 print_const (clause);
	 
      } else if (param_p (clause)) {
	 print_param (clause);
	 
      } else if (or_clause (clause)) {
	 mesg ("(OR ");
	 print_clauses (get_orclauseargs (clause));
	 mesg (") ");
	 
      } else if (not_clause (clause)) {
	 mesg ("(NOT ");
	 print_clause (get_notclausearg (clause), 0);
	 mesg (") ");
	 
      } else if (is_funcclause (clause)) {
	 mesg ("(", get_funcid (get_function (clause)), " ");
	 print_clauses (get_funcargs (clause));
	 mesg (") ");
	 
      } else if (1 /* XXX - true */) {
	 mesg ("(", get_opno (get_op (clause)), " ");
	 print_clause (get_leftop (clause), 0);
	 mesg (" ");
	 print_clause (get_rightop (clause), 0);
	 mesg (") ");
	 
      } else if ( true ) ;  
      end-cond ;
   };
}

/*  .. print_clause
 */
LispValue
print_clauses (clauses)
     LispValue clauses ;
{
   foreach (clause, clauses) {
      print_clause (clause, 0);
      
   };
}

/*  .. print_clause
 */
LispValue
print_const (const)
     LispValue const ;
{
/* Don't know what this is supposed to do
 * I think this should be
 */

  if (is_null (const))
    {
      printf("*NULL*");
    } else if (stringp (get_constvalue (const)))
      {
	printf(%s,get_constvalue (const));
      } else
	{
	  printf(%s, lisp_print (get_constvalue (const)));     /* need to convert lisp -> string here */
	};
}

/*  .. print_clause
 */
LispValue
print_param (param)
LispValue param ;
{
	mesg (" $");
	if (stringp (get_paramid (param)))
	  {
	    mesg(".");
	  } else {
	    mesg("");
	  };
	get_paramid (param);
      }

/*  .. print_clause, print_rtentries, print_subplan, print_tlist
 */
LispValue
indent (levels)
LispValue levels ;
{
	foreach (i, levels)
	  {
	    mesg ("        ");
	  };
      }
