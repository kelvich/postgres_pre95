/*
 *      FILE
 *     	pppp
 *
 *      DESCRIPTION
 *     	POSTGRES plan pretty printer
 *
 */

#ifdef NOT_FOR_DEMO

#include "c.h"
#include "postgres.h"

RcsId("$Header$");

#define	INDENT(c)	printf("%*s", (2 * (c)), "")

/*     
 *      EXPORTS
 *     		print_parse
 *     		print_plan
 *
 *  provide ("pppp");
 */

#include "nodes.h"
#include "plannodes.h"
#include "primnodes.h"
#include "execnodes.h"
#include "planner/internal.h";
#include "pg_lisp.h"

extern	void	print_root();
extern	void	print_clauses();
extern	void	print_const();
extern	void	print_subplan();
extern	void	print_rtentries();
extern	void	print_tlist();
extern	void	print_tlistentry();
extern	void	print_allqual();
extern	void	print_qual();
extern	void	print_clause();
extern	void	print_param();

/*  #ifdef opus43
 *  declare (special (_newline_))
 *  #endif
 */

/* #ifdef (opus43 && pg_production)
 * declare (localf (print_root, print_subplan, print_rtentries,
 *		 print_tlist, print_tlistentry, print_allqual,
 *		 print_qual, print_var, print_clause, print_clauses,
 *		 print_const, print_param,indent)
 *      )
 * #endif
 */
void
print_parse (parse)
	LispValue parse;
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
void
print_root (root)
	LispValue root;
{
	printf ("NumLevels: %s\nCommandType: %s\nResultRelation: %s\n Rangetable:\n",
		root_numlevels(root), root_command_type(root),
		root_result_relation (root),
	      "RangeTable:\n");
	print_rtentries (root_rangetable (root), 0);
	printf ("Priority: %s\n RuleInfo: %s\n", root_priority(root),
		root_ruleinfo(root));
}

/*  .. print_plan, print_subplan
 */
void
print_plan(plan, levelnum)
	LispValue plan;
	int levelnum;
{
	int i;
	LispValue saved_rt;
	extern LispValue copy_seq_tree();
	extern LispValue fix_rangetable();

	if (IsA(plan,Append)) {
		INDENT(levelnum);
		printf("Append %d\n\nInheritance Relid : %ld\n", levelnum,
			get_unionrelid(plan));

		saved_rt = copy_seq_tree (_query_range_table_);

		for (i = 0; i < (length(get_unionplans(plan))); i++){
			INDENT(levelnum);
			printf("Appended Plan %d\n", i);
			_query_range_table_ =
				fix_rangetable(_query_range_table_,
						get_unionrelid (plan),
						nth(i, get_rtentries(plan)));
			print_plan(nth (i, get_unionplans(plan)), levelnum + 1);
		};

		/* XXX need to free the munged tree */
		_query_range_table_ = saved_rt;

		INDENT(levelnum);
		printf("Inheritance Range Table :\n");
		print_rtentries (get_rtentries (plan), levelnum);

	} else if (IsA(plan,Existential)) {
		INDENT(levelnum);
		printf("Existential %d\n", levelnum);

		if (get_lefttree(plan)) {
			INDENT(levelnum);
			printf("\nExistential Plan %d\n", levelnum);
			print_plan (get_lefttree (plan), levelnum + 1);
		};

		if (get_righttree(plan)) {
			INDENT(levelnum);
			printf("\nQuery Plan %d\n", levelnum);
			print_plan (get_righttree (plan), levelnum + 1);
		};

	} else if (IsA(plan,Existential)) {
		INDENT(levelnum);
		printf("Result %d\n\n", levelnum);

		INDENT(levelnum);
		printf("TargetList :\n");
		print_tlist(get_qptargetlist(plan), levelnum + 1);

		INDENT(levelnum);
		printf("RelLevelQual :\n");
		print_qual(get_resrellevelqual(plan), levelnum + 1);

		INDENT(levelnum);
		printf("ConstantQual :\n");
		print_qual(get_resconstantqual(plan), levelnum + 1);

		if (get_lefttree(plan)) {
			INDENT(levelnum);
			printf("\nSubplan %d\n\n", levelnum);
			print_subplan(get_lefttree(plan), levelnum + 1);
		};

		if (get_righttree(plan)) {
			print_plan(get_righttree(plan), (levelnum + 1));
		};
	} else {

		/*
		 *  Better hope we get this right.
		 */

		printf("Subplan %d\n\n", levelnum);
		print_subplan(plan, levelnum + 1);
	}
}

/*  .. print_plan, print_subplan
 */
void
print_subplan (subplan, levels)
	LispValue subplan;
	int levels;
{
	INDENT(levels);
	printf("Type : %d\n", ((Node)subplan)->type);
	INDENT (levels);
	printf("Cost : %g\n", get_state(subplan));
	if (IsA(subplan,Scan)) {
		INDENT (levels);
		printf("Relid: ");

		/* XXX help, jeff, what's integerp become? */
		/*
		 * if (integerp (get_scanrelid (subplan)) &&
		 *     (_TEMP_RELATION_ID_ != get_scanrelid (subplan)))
		 */
		if (_TEMP_RELATION_ID_ != get_scanrelid(subplan)) {
			printf( "%s", getrelname(get_scanrelid(subplan),
						 _query_range_table_));
		} else {
			printf("%ld", get_scanrelid (subplan));
		};
		printf("\n");
	};
	if (IsA(subplan,Temp)) {
		INDENT (levels);
		printf("TempID: %d\n", get_tempid(subplan));
		INDENT (levels);
		printf("KeyCount: %d\n", get_keycount(subplan));
		INDENT (levels);
		printf ("TempList:\n");
		print_tlist(get_qptargetlist(subplan), (levels + 1)); 

	} else {
		INDENT(levels);
		printf("TargetList :\n");
		print_tlist(get_qptargetlist(subplan), (levels + 1));

		INDENT (levels);
		printf("Qual:\n");
		print_qual(get_qpqual(subplan), (levels + 1));
	}

	if (IsA(subplan,MergeJoin)) {
		INDENT (levels);
		printf("MergeSortOp: %ld\n", get_mergesortop(subplan));

		INDENT (levels);
		printf("MergeClauses :\n");
		print_qual(get_mergeclauses(subplan), (levels + 1));
	};
	if (IsA(subplan,IndexScan)) {
		/*
		 *  XXX need to add a function to print list at
		 *  the current nesting level.  until then...
		 *
		 * INDENT(levels);
		 * printf("IndexID: XXX", get_indxid (subplan), "\n");
		 */

		INDENT(levels);
		printf("IndexQual:\n");
		print_allqual(get_indxqual (subplan), (levels + 1));

		printf ("\n");
	};
	if (IsA(subplan,Join)) {
		INDENT(levels);
		printf("Outer Path:\n");
		print_subplan(get_lefttree(subplan), (levels + 1));

		INDENT(levels);
		printf("Inner Path:\n");
		print_subplan(get_righttree(subplan), (levels + 1));

	} else if (get_lefttree(subplan)) {
		printf ("\n");
		if (IsA(get_lefttree(subplan),Result)) {
			print_plan(get_lefttree(subplan), levels + 1);
		} else {
			print_subplan(get_lefttree(subplan), levels + 1);
		}
	}
}

/*  .. print_plan, print_root
 */
void
print_rtentries (rt, levels)
	LispValue rt;
	int levels;
{
	LispValue i;
	LispValue rtentry;

	/* XXX how do we re-write foreach loops? */
	foreach (i, rt) {
		rtentry = CAR(i);

		INDENT (levels + 1);
		printf("(%s %ld :time %ld :flags 0x%lx :rulelocks",
			rt_relname(rtentry),
		        rt_relid(rtentry),
		        rt_time(rtentry),
		        rt_flags(rtentry));

#ifdef NOTYET
		/* XXX integerp becomes what? */
		if (integerp (rt_rulelocks (rtentry))) {
			printf ("\n");
			INDENT (levels + 1);
			print_rule_lock_intermediate(rt_rulelocks(rtentry));
		} else {
			/*
			 *  XXX what the hell does this do???
			 *
			 *  printf (rt_rulelocks (rtentry));
			 */
		}
#endif /* NOTYET */
		printf (")\n");
	}
}

/*  .. print_parse, print_plan, print_subplan */
void
print_tlist (tlist, levels)
     LispValue tlist;
     int levels;
{
	LispValue entry;
	LispValue i;

	foreach (i, tlist) {
		entry = CAR(i);
		print_tlistentry(entry, (levels + 1));
	}
}

/*  .. print_tlist 
*/
void
print_tlistentry(tlistentry, level)
	LispValue tlistentry;
	int level;
{
	LispValue resdom;
	LispValue expr;

	resdom = (LispValue) tl_resdom(tlistentry);
	expr = (LispValue) get_expr(tlistentry);

	INDENT(level);
	printf ("(%d ", get_resno(resdom));

	if (get_reskey(resdom) != 0) {
		/*
		 *  XXX should print the tuple form, but we just print the
		 *  address for now.
		 */
		printf ("(%d 0x%lx)",
			get_reskey(resdom), get_reskeyop(resdom));
	};
	/*
	 * if (get_resname(resdom) != (char *) NULL) {
	 * 	printf ("%s", get_resname(resdom));
	 * };
	 */
	print_clause (expr, level);
	printf (")\n");
}

/*  .. print_subplan 
*/

void
print_allqual(quals, levels)
	LispValue quals;
	int levels;
{
	LispValue qual;
	LispValue i;

	/* XXX how do we do foreach() loops?  */
	foreach (i, quals) {
		qual = CAR(i);
		print_qual(qual,levels + 1);
		printf ("\n");
	};
}

/*  .. print_allqual, print_parse, print_plan, print_subplan
 */

void
print_qual(qual, levels)
	LispValue qual;
	int levels;
{
	LispValue clause;
	LispValue i;

	foreach (i, qual) {
		qual = CAR(i);
		print_clause(clause, levels);
		printf ("\n");
	};
}

/*  .. print_clause
 */
void
print_var(var)
	LispValue var;
{
	printf ("(");
	if(_query_range_table_ && (_TEMP_RELATION_ID_ != get_varno(var)))
	{
		printf("%s %s",
			getrelname(get_varno(var),_query_range_table_),
			get_attname(getrelid(get_varno(var),
				    _query_range_table_),
				    get_varattno (var)));

	} else {
		printf ("%d %d", get_varno(var), get_varattno(var));
	}

	/* XXX foreach */
	/*
	 *  foreach (dot, get_vardotfields (var)) {
	 * 	printf (" ", dot);
	 * };
	 */

	if (get_vararrayindex(var) != 0) {
		printf ("(%d)", get_vararrayindex(var));
	};

	printf (")");
}

/*  .. print_clause, print_clauses, print_qual, print_tlistentry
 */
void
print_clause(clause, levels)
	LispValue clause;
	int levels;
{
	if (clause) {
		INDENT (levels);
		if (IsA(clause,Var)) {
			print_var (clause);

		} else if (IsA(clause,Const)) {
			print_const (clause);

		} else if (IsA(clause,Param)) {
			print_param (clause);

		/*
		 * XXX don't know about these node types.
		 *
		 * } else if (or_clause (clause)) {
		 * 	printf ("(OR ");
		 * 	print_clauses (get_orclauseargs (clause));
		 * 	printf (") ");
		 *
		 * } else if (not_clause (clause)) {
		 * 	printf ("(NOT ");
		 * 	print_clause (get_notclausearg (clause), 0);
		 * 	printf (") ");
		 *
		 * } else if (is_funcclause (clause)) {
		 * 	printf ("(", get_funcid (get_function (clause)), " ");
		 * 	print_clauses (get_funcargs (clause));
		 * 	printf (") ");
		 */

		 } else {
		 	printf("(%d ", get_opno(get_op(clause)));
		 	print_clause(get_leftop(clause), levels);
		 	printf(" ");
			print_clause(get_rightop(clause), levels);
			printf(") ");
		}
	}
}

/*  .. print_clause
 */
/*
 *  XXX this routine no longer needed.
 *
 *
 * LispValue
 * print_clauses (clauses)
 *      LispValue clauses ;
 * {
 *    foreach (clause, clauses) {
 *       print_clause (clause, 0);
 *       
 *    };
 * }
 */

/*  .. print_clause
 */
void
print_const(node)
	LispValue node;
{
	if (get_constisnull(node)) {
		printf("*NULL*");
	/*
	 * } else if (stringp (get_constvalue (node))) {
	 * 	printf("%s",get_constvalue (node));
	 * } else {
	 * 	printf("%s", lisp_print (get_constvalue(node)));
	 * }
	 */

	} else {
		/*
		 *  XXX -- neet to figure out what type of constant this
		 *  is, and print it for real.
		 */
		printf("%ld", DatumGetUInt32(((Const)node)->constvalue));
	}
}

/*  .. print_clause
 */
void
print_param (param)
	LispValue param;
{
	extern char *get_paramname();

	printf (" $%ld", get_paramid(param));

	/*
	 * if (stringp (get_paramid (param))) {
	 * 	printf(".");
	 * } else {
	 * 	printf("");
	 * }
	 */

	 if (get_paramname(param) != (char *) NULL)
		printf("(\"%s\")", get_paramname(param));
}

#endif /* NOT_FOR_DEMO */
