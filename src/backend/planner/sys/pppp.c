/*
 *      FILE
 *     	pppp
 *
 *      DESCRIPTION
 *     	POSTGRES plan pretty printer
 *
 */

#include "tmp/postgres.h"

RcsId("$Header$");

#define	INDENT(c)	printf("%*s", (2 * (c)), "")

/*     
 *      EXPORTS
 *     		print_parse
 *     		print_plan
 *
 *  provide ("pppp");
 */

#include "nodes/nodes.h"
#include "nodes/pg_lisp.h"
#include "nodes/plannodes.h"
#include "nodes/plannodes.a.h"
#include "nodes/primnodes.h"
#include "nodes/primnodes.a.h"
#include "nodes/execnodes.h"
#include "nodes/relation.h"
#include "nodes/relation.a.h"
#include "tags.h"

#include "planner/internal.h"
#include "planner/clauses.h"
#include "planner/keys.h"

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
	printf ("NumLevels: %d\nCommandType: %d\nResultRelation: ",
		root_numlevels(root), root_command_type(root));
	lispDisplay(root_result_relation (root), 0);
	printf("\nRangetable:\n");
	print_rtentries (root_rangetable (root), 0);
	printf ("Priority: %d\nRuleInfo: ", CInteger(root_priority(root)));
	lispDisplay(root_ruleinfo(root), 0);
	printf("\n");
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
						nth(i, get_unionrtentries(plan)));
			print_plan(nth (i, get_unionplans(plan)), levelnum + 1);
		};

		/* XXX need to free the munged tree */
		_query_range_table_ = saved_rt;

		INDENT(levelnum);
		printf("Inheritance Range Table :\n");
		print_rtentries (get_unionrtentries (plan), levelnum);

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

char*
subplan_type(type)
int type;
{
	switch (type) {
	case T_NestLoop:  return("NestLoop");
	case T_MergeJoin: return("MergeJoin");
	case T_HashJoin:  return("HashJoin");
	case T_SeqScan:	return("SeqScan");
	case T_IndexScan:	return("IndexScan");
	case T_Sort:	return("Sort");
	case T_Hash:	return("Hash");
	case T_Material:return("Material");
	case T_Temp:	return("Temp");
	case T_Append:	return("Append");
	case T_Result:	return("Result");
	case T_Existential: return("Existential");
	case T_ScanTemps: return("ScanTemps");
	default: return("???");
	}
}

/*  .. print_plan, print_subplan
 */
void
print_subplan (subplan, levels)
	LispValue subplan;
	int levels;
{
	void print_var();

	INDENT(levels);
	printf("Type : %s\n", subplan_type(((Node)subplan)->type));
	INDENT (levels);
	printf("Cost : %f\n", get_cost(subplan));
	INDENT (levels);
	printf("Size : %d\n", get_plan_size(subplan));
	INDENT (levels);
	printf("Width : %d\n", get_plan_width(subplan));
	if (IsA(subplan,Scan)) {
		INDENT (levels);
		printf("Relid: ");

		/* XXX help, jeff, what's integerp become? */
		/*
		 * if (integerp (get_scanrelid (subplan)) &&
		 *     (_TEMP_RELATION_ID_ != get_scanrelid (subplan)))
		 */
		if (_TEMP_RELATION_ID_ != get_scanrelid(subplan)) {
			printf( "%s", CString(getrelname(get_scanrelid(subplan),
						 _query_range_table_)));
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

	} else if (IsA(subplan,Hash)) {
		INDENT(levels);
		printf("HashKey: ");
		print_var(get_hashkey(subplan));
	} else  {
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
	}
	if (IsA(subplan,HashJoin)) {
		INDENT (levels);
		printf("hashClauses :\n");
		print_qual(get_hashclauses(subplan), (levels + 1));
	}
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
		printf("\n");
		INDENT(levels);
		printf("Outer Path:\n");
		print_subplan(get_lefttree(subplan), (levels + 1));
		printf("\n");
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
		lispDisplay(rtentry,0);

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
		printf ("\n");
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

	INDENT (levels);
	foreach (i, qual) {
		clause = CAR(i);
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
	Index varno;
	AttributeNumber attnum;
	printf ("(");
	if(_query_range_table_ && (_TEMP_RELATION_ID_ != get_varno(var)))
	{
		varno = get_varno(var);
		attnum = get_varattno (var);
		if (varno == INNER || varno == OUTER) {
		   List varid = get_varid(var);
		   varno = CInteger(CAR(varid));
		   attnum = CInteger(CADR(varid));
		  }
		printf("%s %s",
			CString(getrelname(varno, _query_range_table_)),
		        get_attname(CInteger(getrelid(varno,
				 _query_range_table_)), attnum));

	} else {
		printf ("%d %d", get_varno(var), get_varattno(var));
	}

	/* XXX foreach */
	/*
	 *  foreach (dot, get_vardotfields (var)) {
	 * 	printf (" ", dot);
	 * };
	 */

	/* XXX - is this what we want? */
/* 	if (get_vararraylist(var) != NULL) {
		List elem;
	 	foreach (elem, get_vararraylist (var)) { 
	 		printf ("(%d)", elem->arraylow); 
	 	};
	};
*/
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

	printf (" $%ld", get_paramid(param));

	/*
	 * if (stringp (get_paramid (param))) {
	 * 	printf(".");
	 * } else {
	 * 	printf("");
	 * }
	 */

	 if (get_paramname(param) != (Name) NULL)
		printf("(\"%s\")", get_paramname(param));
}

void
pplan(plan)
Plan plan;
{
    if (plan == NULL) return;
    fprintf(stderr, "(%s ", subplan_type(plan->type));
    switch (NodeType(plan)) {
    case classTag(SeqScan):
    case classTag(IndexScan):
	if (_TEMP_RELATION_ID_ != get_scanrelid(plan)) {
	    fprintf(stderr,  "%s", CString(getrelname(get_scanrelid(plan),
				  _query_range_table_)));
	  }
	else {
	    pplan(get_lefttree(plan));
	  }
	break;
    case classTag(ScanTemps):
	{   LispValue x;
	    Relation tmpreldesc;
	    foreach (x, get_temprelDescs(plan)) {
		tmpreldesc = (Relation)CAR(x);
		fprintf(stderr, " %s", &(tmpreldesc->rd_rel->relname));
	      }
	}
    case classTag(MergeJoin):
    case classTag(HashJoin):
    case classTag(NestLoop):
	pplan(get_lefttree(plan));
	fprintf(stderr, " ");
	pplan(get_righttree(plan));
	break;
    case classTag(Hash):
    case classTag(Sort):
    case classTag(Material):
    case classTag(Result):
	pplan(get_lefttree(plan));
	break;
    case classTag(Existential):
	pplan(get_lefttree(plan));
	fprintf(stderr, " ");
	pplan(get_righttree(plan));
	break;
    case classTag(Append):
	{  LispValue saved_rt;
	   extern LispValue copy_seq_tree();
	   extern LispValue fix_rangetable();
	   int i;

	   saved_rt = copy_seq_tree(_query_range_table_);
	   for (i=0; i<(length(get_unionplans(plan))); i++) {
	       _query_range_table_ = fix_rangetable(_query_range_table_,
					 get_unionrelid(plan),
					 nth(i,get_unionrtentries(plan)));
	       pplan(nth(i, get_unionplans(plan)));
	     }
	    _query_range_table_ = saved_rt;
	    break;
	 }
     default:
	 fprintf(stderr, "unknown plan type.\n");
	 break;
      }
     fprintf(stderr, ")");
     return;
}

void
p_plan(plan)
Plan plan;
{
    fprintf(stderr, "\n");
    pplan(plan);
    fprintf(stderr, "\n");
}

void
p_plans(plans)
List plans;
{
    LispValue x;

    foreach (x, plans) {
	p_plan(CAR(x));
      }
}

char *
path_type(path)
Path path;
{
	switch (NodeType(path)) {
	case classTag(HashPath): return "HashJoin";
	case classTag(MergePath): return "MergeJoin";
	case classTag(JoinPath): return "NestLoop";
	case classTag(IndexPath): return "IndexScan";
	case classTag(Path): return "SeqScan";
	default: return "???";
	}
}

void
ppath(path)
Path path;
{
    if (path == NULL) return;
    fprintf(stderr, "(%s ", path_type(path));
    switch (NodeType(path)) {
    case classTag(HashPath):
	ppath(get_outerjoinpath(path));
	fprintf(stderr, " (Hash ");
	ppath(get_innerjoinpath(path));
	fprintf(stderr, ")");
	break;
    case classTag(MergePath):
	if (get_outersortkeys(path)) {
	    fprintf(stderr, "(SeqScan (Sort ");
	    ppath(get_outerjoinpath(path));
	    fprintf(stderr, "))");
	  }
	else
	    ppath(get_outerjoinpath(path));
	fprintf(stderr, " ");
	if (get_innersortkeys(path)) {
	    fprintf(stderr, "(SeqScan (Sort ");
	    ppath(get_innerjoinpath(path));
	    fprintf(stderr, "))");
	  }
	else
	    ppath(get_innerjoinpath(path));
	break;
    case classTag(JoinPath):
	ppath(get_outerjoinpath(path));
	fprintf(stderr, " ");
	ppath(get_innerjoinpath(path));
	break;
    case classTag(IndexPath):
    case classTag(Path):
	fprintf(stderr, "%s", 
	  CString(getrelname(CInteger(CAR(get_relids(get_parent(path)))),
			     _query_range_table_)));
	break;
    default:
	fprintf(stderr, "unknown plan type.\n");
	break;
      }
    fprintf(stderr, ")");
    return;
}

void
p_path(path)
Path path;
{
    ppath(path);
    fprintf(stderr, "\n");
}

void
p_paths(paths)
List paths;
{
    LispValue x;

    foreach (x, paths) {
	p_path(CAR(x));
      }
}

void
p_rel(rel)
Rel rel;
{
    fprintf(stderr, "relids: ");
    lispDisplay(get_relids(rel));
    fprintf(stderr, "\npathlist:\n");
    p_paths(get_pathlist(rel));
    fprintf(stderr, "unorderedpath:\n");
    p_path(get_unorderedpath(rel));
    fprintf(stderr, "cheapestpath:\n");
    p_path(get_cheapestpath(rel));
}

void
p_rels(rels)
List rels;
{
    LispValue x;

    foreach (x, rels) {
	p_rel(CAR(x));
      }
}
