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
#include "planner/prepqual.h"

void print_parse ARGS((LispValue parse ));
void print_root ARGS((LispValue root ));
void print_plan ARGS((Plan plan , int levelnum ));
char *subplan_type ARGS((int type ));
void print_subplan ARGS((LispValue subplan , int levels ));
void print_rtentries ARGS((LispValue rt , int levels ));
void print_tlist ARGS((LispValue tlist , int levels ));
void print_tlistentry ARGS((LispValue tlistentry , int level ));
void print_allqual ARGS((LispValue quals , int levels ));
void print_qual ARGS((LispValue qual , int levels ));
void print_var ARGS((LispValue var ));
void print_clause ARGS((LispValue clause , int levels ));
void print_const ARGS((LispValue node ));
void print_param ARGS((LispValue param ));
void pplan ARGS((Plan plan ));
void set_query_range_table ARGS((List parsetree ));
void p_plan ARGS((Plan plan ));
void p_plans ARGS((List plans ));
char *path_type ARGS((Path path ));
void ppath ARGS((Path path ));
void p_path ARGS((Path path ));
void p_paths ARGS((List paths ));
void p_rel ARGS((Rel rel ));
void p_rels ARGS((List rels ));

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
	print_qual (cnfify (parse_qualification (parse), false),0);
}

/*  .. print_parse
 */
void
print_root (root)
	LispValue root;
{
	printf ("NumLevels: %d\nCommandType: %d\nResultRelation: ",
		root_numlevels(root), root_command_type(root));
	lispDisplay(root_result_relation (root));
	printf("\nRangetable:\n");
	print_rtentries (root_rangetable (root), 0);
	printf ("Priority: %d\nRuleInfo: ", CInteger(root_priority(root)));
	lispDisplay(root_ruleinfo(root));
	printf("\n");
}

/*  .. print_plan, print_subplan
 */
void
print_plan(plan, levelnum)
	Plan plan;
	int levelnum;
{
	int i;
	LispValue saved_rt;
	extern LispValue copy_seq_tree();
	extern LispValue fix_rangetable();

	if (IsA(plan,Append)) {
		INDENT(levelnum);
		printf("Append %d\n\nInheritance Relid : %ld\n", levelnum,
			get_unionrelid((Append)plan));

		saved_rt = copy_seq_tree (_query_range_table_);

		for (i = 0; i < (length(get_unionplans((Append)plan))); i++){
			INDENT(levelnum);
			printf("Appended Plan %d\n", i);
			_query_range_table_ =
			    fix_rangetable(_query_range_table_,
				    get_unionrelid ((Append)plan),
				    nth(i, get_unionrtentries((Append)plan)));
			print_plan((Plan)nth(i, get_unionplans((Append)plan)),
				   levelnum + 1);
		};

		/* XXX need to free the munged tree */
		_query_range_table_ = saved_rt;

		INDENT(levelnum);
		printf("Inheritance Range Table :\n");
		print_rtentries ((LispValue)get_unionrtentries((Append)plan), levelnum);

	} else if (IsA(plan,Existential)) {
		INDENT(levelnum);
		printf("Existential %d\n", levelnum);

		if (get_lefttree((Plan)plan)) {
			INDENT(levelnum);
			printf("\nExistential Plan %d\n", levelnum);
			print_plan ((Plan)get_lefttree (plan), 
				    levelnum + 1);
		};

		if (get_righttree((Plan)plan)) {
			INDENT(levelnum);
			printf("\nQuery Plan %d\n", levelnum);
			print_plan ((Plan)get_righttree (plan),
				    levelnum + 1);
		};

	} else if (IsA(plan,Existential)) {
		INDENT(levelnum);
		printf("Result %d\n\n", levelnum);

		INDENT(levelnum);
		printf("TargetList :\n");
		print_tlist(get_qptargetlist((Plan)plan), levelnum + 1);

		INDENT(levelnum);
		printf("RelLevelQual :\n");
		print_qual(get_resrellevelqual((Result)plan), levelnum+1);

		INDENT(levelnum);
		printf("ConstantQual :\n");
		print_qual(get_resconstantqual((Result)plan), levelnum+1);

		if (get_lefttree(plan)) {
			INDENT(levelnum);
			printf("\nSubplan %d\n\n", levelnum);
			print_subplan((LispValue)get_lefttree(plan),
				      levelnum + 1);
		};

		if (get_righttree(plan)) {
			print_plan((Plan)get_righttree(plan), (levelnum + 1));
		};
	} else {

		/*
		 *  Better hope we get this right.
		 */

		printf("Subplan %d\n\n", levelnum);
		print_subplan((LispValue)plan, levelnum + 1);
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
	INDENT(levels);
	printf("Type : %s\n", subplan_type(((Node)subplan)->type));
	INDENT (levels);
	printf("Cost : %f\n", get_cost((Plan)subplan));
	INDENT (levels);
	printf("Size : %d\n", get_plan_size((Plan)subplan));
	INDENT (levels);
	printf("Width : %d\n", get_plan_width((Plan)subplan));
	if (IsA(subplan,Scan)) {
		INDENT (levels);
		printf("Relid: ");

		/* XXX help, jeff, what's integerp become? */
		/*
		 * if (integerp (get_scanrelid (subplan)) &&
		 *     (_TEMP_RELATION_ID_ != get_scanrelid (subplan)))
		 */
		if (_TEMP_RELATION_ID_ != get_scanrelid((Scan)subplan)) {
			printf( "%s",
				CString(getrelname(get_scanrelid((Scan)subplan),
						   _query_range_table_)));
		} else {
			printf("%ld", get_scanrelid ((Scan)subplan));
		};
		printf("\n");
	};
	if (IsA(subplan,Temp)) {
		INDENT (levels);
		printf("TempID: %d\n", get_tempid((Temp)subplan));
		INDENT (levels);
		printf("KeyCount: %d\n", get_keycount((Temp)subplan));
		INDENT (levels);
		printf ("TempList:\n");
		print_tlist(get_qptargetlist((Plan)subplan), (levels + 1)); 

	} else if (IsA(subplan,Hash)) {
		INDENT(levels);
		printf("HashKey: ");
		print_var((LispValue)get_hashkey((Hash)subplan));
	} else  {
		INDENT(levels);
		printf("TargetList :\n");
		print_tlist((LispValue)get_qptargetlist((Plan)subplan), (levels + 1));

		INDENT (levels);
		printf("Qual:\n");
		print_qual((LispValue)get_qpqual((Plan)subplan), (levels + 1));
	}

	if (IsA(subplan,MergeJoin)) {
		INDENT (levels);
		printf("MergeSortOp:%ld\n",get_mergesortop((MergeJoin)subplan));

		INDENT (levels);
		printf("MergeClauses :\n");
		print_qual(get_mergeclauses((MergeJoin)subplan), (levels + 1));
	}
	if (IsA(subplan,HashJoin)) {
		INDENT (levels);
		printf("hashClauses :\n");
		print_qual(get_hashclauses((HashJoin)subplan), (levels + 1));
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
		print_allqual(get_indxqual ((IndexScan)subplan), (levels + 1));

		printf ("\n");
	};
	if (IsA(subplan,Join)) {
		printf("\n");
		INDENT(levels);
		printf("Outer Path:\n");
		print_subplan((LispValue)get_lefttree((Plan)subplan),
			      (levels + 1));
		printf("\n");
		INDENT(levels);
		printf("Inner Path:\n");
		print_subplan((LispValue)get_righttree((Plan)subplan), (levels + 1));

	} else if (get_lefttree((Plan)subplan)) {
		printf ("\n");
		if (IsA(get_lefttree((Plan)subplan),Result)) {
			print_plan((Plan)get_lefttree((Plan)subplan), levels + 1);
		} else {
			print_subplan((LispValue)get_lefttree((Plan)subplan), levels + 1);
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
		lispDisplay(rtentry);

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
	printf ("(%d ", get_resno((Resdom)resdom));

	if (get_reskey((Resdom)resdom) != 0) {
		/*
		 *  XXX should print the tuple form, but we just print the
		 *  address for now.
		 */
		printf("(%d 0x%lx)",
		    get_reskey((Resdom)resdom),get_reskeyop((Resdom)resdom));
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
	if(_query_range_table_ && (_TEMP_RELATION_ID_ != get_varno((Var)var)))
	{
		varno = get_varno((Var)var);
		attnum = get_varattno ((Var)var);
		if (varno == INNER || varno == OUTER) {
		   List varid = get_varid((Var)var);
		   varno = CInteger(CAR(varid));
		   attnum = CInteger(CADR(varid));
		  }
		printf("%s %s",
			CString(getrelname(varno, _query_range_table_)),
		        get_attname(CInteger(getrelid(varno,
				 _query_range_table_)), attnum));

	} else {
		printf ("%d %d", get_varno((Var)var), get_varattno((Var)var));
	}
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
		 	printf("(%d ", get_opno((Oper)get_op(clause)));
		 	print_clause((LispValue)get_leftop(clause), levels);
		 	printf(" ");
			print_clause((LispValue)get_rightop(clause), levels);
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
	if (get_constisnull((Const)node)) {
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

	printf (" $%ld", get_paramid((Param)param));

	/*
	 * if (stringp (get_paramid (param))) {
	 * 	printf(".");
	 * } else {
	 * 	printf("");
	 * }
	 */

	 if (get_paramname((Param)param) != (Name) NULL)
		printf("(\"%s\")", get_paramname((Param)param));
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
	if (_TEMP_RELATION_ID_ != get_scanrelid((Scan)plan)) {
	    fprintf(stderr,  "%s", CString(getrelname(get_scanrelid((Scan)plan),
				  _query_range_table_)));
	  }
	else {
	    pplan((Plan)get_lefttree(plan));
	  }
	break;
    case classTag(ScanTemps):
	{   LispValue x;
	    Relation tmpreldesc;
	    foreach (x, get_temprelDescs((ScanTemps)plan)) {
		tmpreldesc = (Relation)CAR(x);
		fprintf(stderr, " %s", &(tmpreldesc->rd_rel->relname));
	      }
	}
    case classTag(MergeJoin):
    case classTag(HashJoin):
    case classTag(NestLoop):
	pplan((Plan)get_lefttree(plan));
	fprintf(stderr, " ");
	pplan((Plan)get_righttree(plan));
	break;
    case classTag(Hash):
    case classTag(Sort):
    case classTag(Material):
    case classTag(Result):
	pplan((Plan)get_lefttree(plan));
	break;
    case classTag(Existential):
	pplan((Plan)get_lefttree(plan));
	fprintf(stderr, " ");
	pplan((Plan)get_righttree(plan));
	break;
    case classTag(Append):
	{  LispValue saved_rt;
	   extern LispValue copy_seq_tree();
	   extern LispValue fix_rangetable();
	   int i;

	   saved_rt = copy_seq_tree(_query_range_table_);
	   for (i=0; i<(length(get_unionplans((Append)plan))); i++) {
	       _query_range_table_ = 
		    fix_rangetable(_query_range_table_,
				   get_unionrelid((Append)plan),
				   nth(i,get_unionrtentries((Append)plan)));
	       pplan((Plan)nth(i, get_unionplans((Append)plan)));
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
set_query_range_table(parsetree)
List parsetree;
{
    _query_range_table_ = root_rangetable(parse_root(parsetree));
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
	p_plan((Plan)CAR(x));
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
	ppath((Path)get_outerjoinpath((JoinPath)path));
	fprintf(stderr, " (Hash ");
	ppath((Path)get_innerjoinpath((JoinPath)path));
	fprintf(stderr, ")");
	break;
    case classTag(MergePath):
	if (get_outersortkeys((MergePath)path)) {
	    fprintf(stderr, "(SeqScan (Sort ");
	    ppath((Path)get_outerjoinpath((JoinPath)path));
	    fprintf(stderr, "))");
	  }
	else
	    ppath((Path)get_outerjoinpath((JoinPath)path));
	fprintf(stderr, " ");
	if (get_innersortkeys((MergePath)path)) {
	    fprintf(stderr, "(SeqScan (Sort ");
	    ppath((Path)get_innerjoinpath((JoinPath)path));
	    fprintf(stderr, "))");
	  }
	else
	    ppath((Path)get_innerjoinpath((JoinPath)path));
	break;
    case classTag(JoinPath):
	ppath((Path)get_outerjoinpath((JoinPath)path));
	fprintf(stderr, " ");
	ppath((Path)get_innerjoinpath((JoinPath)path));
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
	p_path((Path)CAR(x));
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
    p_path((Path)get_unorderedpath(rel));
    fprintf(stderr, "cheapestpath:\n");
    p_path((Path)get_cheapestpath(rel));
}

void
p_rels(rels)
List rels;
{
    LispValue x;

    foreach (x, rels) {
	p_rel((Rel)CAR(x));
      }
}
