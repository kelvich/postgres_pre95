
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
	printf ("TargetList:\n");
	print_tlist (parse_targetlist (parse),0);
	printf ("Qualification:\n");
	print_qual (cnfify (parse_qualification (parse)),0);
}

/*  .. print_parse
 */
LispValue
print_root (root)
LispValue root ;
{
	printf ("NumLevels: %s\nCommandType: %s\nResultRelation: %s\n Rangetable:\n",
		root_levels(root), root_command_type(root),
		root_result_relation (root),
	      "RangeTable:\n");
	print_rtentries (root_rangetable (root), 0);
	printf ("Priority: %s\n RuleInfo: %s\n", root_priority(root),
		root_ruleinfo(root));
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
		printf("Append %d\n\nInheritance Relation : %s\n", levelnum,
		       get_inhrelid(plan));
		/* XXX - let form, maybe incorrect */
		saved_rt = copy_seq_tree (_query_range_table_);
		for (i = 0; i < (length (get_plans (plan)); i++){
		  printf("Appended Plan %d\n", i);
		  _query_range_table_ = fix_rangetable (_query_range_table_,
							get_inhrelid (plan),
							nth (i, get_rtentries (plan))),
		  print_plan (nth (i, get_plans (plan)),
		    0);
		    };
		_query_range_table_ = saved_rt;
		
		printf("Inheritance Rangetable :\n");
		print_rtentries (get_rtentries (plan), 0);

	} else if (existential_p (plan)) {
		printf("Existential %d\n", levelnum);
		if /*when */ ( get_lefttree (plan)) {
			printf("\nExistential Plan %d\n", levelnum);
			print_plan (get_lefttree (plan), 0);
		};
		if /*when */ ( get_righttree (plan)) {
			printf("\nQuery Plan %d\n", levelnum);
			print_plan (get_righttree (plan), 0);
		};

	} else if (result_p (plan)) {
		printf("Result %d\n\n", levelnum);
		printf("TargetList :\n");
		print_tlist (get_qptargetlist (plan), 0);
		printf("RelLevelQual :\n");
		print_qual (get_resrellevelqual (plan), 0);
		printf("ConstantQual :\n");
		print_qual (get_resconstantqual (plan), 0);
		if /*when */ ( get_lefttree (plan)) {
			printf("\nSubplan %d\n\n", levelnum);
			print_subplan (get_lefttree (plan), 0);
		};
		if /*when */ ( get_righttree (plan)) {
			print_plan (get_righttree (plan), (levelnum + 1));
		};

	} else if (1 /* XXX - true */) {
		printf("Subplan %d\n\n", levelnum);
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
	printf("        PathType : %s\n",type_of(subplan));
	indent (levels);
	printf("        Cost : %s\n", get_state(subplan));
	if /*when */ ( scan_p (subplan)) {
		indent (levels);
		printf("        Relid : ");
		if (integerp (get_scanrelid (subplan)) &&
		    (_TEMP_RELATION_ID_ != get_scanrelid (subplan)))
		  {
		  printf( "%s", getrelname( get_scanrelid(subplan),_query_range_table_));

		} else {
		  printf("%d", get_scanrelid (subplan));
		};
		printf("\n");
	};
	if (temp_p (subplan)) {
		indent (levels);
		printf("        TempID : %d\n", get_tempid (subplan));
		indent (levels);
		printf("        KeyCount : ", get_keycount (subplan), "\n");
		indent (levels);
		printf ("        TempList :\n");
		print_tlist (get_qptargetlist (subplan), (levels + 1)); 

	} else if (1 /* XXX - true */) {
		indent (levels);
		printf ("        TargetList :\n");
		print_tlist (get_qptargetlist (subplan), (levels + 1));
		indent (levels);
		printf ("        Qual :\n");
		print_qual (get_qpqual (subplan), (levels + 1));

	} else if ( true ) ;  
	end-cond ;
	if /*when */ ( mergesort_p (subplan)) {
		indent (levels);
		printf ("        MergeSortOp : ", get_mergesortop (subplan), "\n");
		indent (levels);
		printf ("        MergeClauses :\n");
		print_qual (get_mergehashclauses (subplan), (levels + 1));
	};
	if /*when */ ( indexscan_p (subplan)) {
		indent (levels);
		printf ("        IndexID : ", get_indxid (subplan), "\n");
		indent (levels);
		printf ("        IndexQual :\n");
		print_allqual (get_indxqual (subplan), (levels + 1));
		printf ("\n");
	};
	if (join_p (subplan)) {
		indent (levels);
		printf ("        OuterPath :\n");
		print_subplan (get_lefttree (subplan), (levels + 1));
		indent (levels);
		printf ("        InnerPath :\n");
		print_subplan (get_righttree (subplan), (levels + 1));

	} else if (get_lefttree (subplan)) {
		printf ("\n");
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
		printf ("(", rt_relname (rtentry),
		      " ", rt_relid (rtentry),
		      " :time ", rt_time (rtentry),
		      " :flags ", rt_flags (rtentry),
		      " :rulelocks ");
		if (integerp (rt_rulelocks (rtentry))) {
			printf ("\n", " ");
			indent (levels + 1);
			print_rule_lock_intermediate (rt_rulelocks (rtentry));
			indent (levels + 1);

		} else if (1 /* XXX - true */) {
			printf (rt_rulelocks (rtentry));

		} else if ( true ) ;  
		end-cond ;
		printf (")\n");

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
   printf ("(", get_resno (resdom)," ");

   if /*when */ (get_reskey (resdom)) {                    /* not equal to 0 */
      printf ("(", get_reskey (resdom), " ", get_reskeyop (resdom), ") ");
   };
   if /*when */ ( get_resname (resdom)) {
      printf (get_resname (resdom), " ");
   };
   print_clause (expr, 0);
   printf (")\n");

}

/*  .. print_subplan 
*/
LispValue
print_allqual (quals, levels)
     LispValue quals, levels ;
{
   foreach (qual, quals) {
      print_qual (qual,levels);
      printf ("\n");
      
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
      printf ("        ");
      print_clause (clause, levels);
      printf ("\n");

   };
}
   
/*  .. print_clause
 */
LispValue
print_var (var)
     LispValue var ;
{
   printf ("(");
   if(_query_range_table_ &&
      integerp (get_varno (var)) &&
      (_TEMP_RELATION_ID_ != get_varno (var)))
      {
      printf (getrelname (get_varno (var),_query_range_table_), " ",
	    get_attname (getrelid (get_varno (var),_query_range_table_),
			 get_varattno (var)));

   } else if (1 /* XXX - true */) {
      printf (get_varno (var), " ", get_varattno (var));

   } else if ( true ) ;  
   end-cond ;
   
   foreach (dot, get_vardotfields (var)) {
      printf (" ", dot);
   };
   if /*when */ ( get_vararrayindex (var)) {
      printf ("(", get_vararrayindex (var), ")");
   };
   printf (")");
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
	 printf ("(OR ");
	 print_clauses (get_orclauseargs (clause));
	 printf (") ");
	 
      } else if (not_clause (clause)) {
	 printf ("(NOT ");
	 print_clause (get_notclausearg (clause), 0);
	 printf (") ");
	 
      } else if (is_funcclause (clause)) {
	 printf ("(", get_funcid (get_function (clause)), " ");
	 print_clauses (get_funcargs (clause));
	 printf (") ");
	 
      } else if (1 /* XXX - true */) {
	 printf ("(", get_opno (get_op (clause)), " ");
	 print_clause (get_leftop (clause), 0);
	 printf (" ");
	 print_clause (get_rightop (clause), 0);
	 printf (") ");
	 
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
	printf (" $");
	if (stringp (get_paramid (param)))
	  {
	    printf(".");
	  } else {
	    printf("");
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
	    printf ("        ");
	  };
      }
