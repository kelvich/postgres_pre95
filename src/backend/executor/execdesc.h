/* ----------------------------------------------------------------
 *   FILE
 *     	execdesc.h
 *     
 *   DESCRIPTION
 *     	parse, plan and query descriptor accessor macros used
 *      by the executor and related modules.
 *
 *   IDENTIFICATION
 *	$Header$"
 * ----------------------------------------------------------------
 */

#ifndef ExecdescHIncluded
#define ExecdescHIncluded 1	/* include only once */

/* ----------------
 *	query descriptor accessors
 * ----------------
 */
#define GetOperation(queryDesc)	     (List) CAR(queryDesc)
#define QdGetParseTree(queryDesc)    (List) CAR(CDR(queryDesc))
#define QdGetPlan(queryDesc)	     (Plan) CAR(CDR(CDR(queryDesc)))
#define QdGetState(queryDesc)	   (EState) CAR(CDR(CDR(CDR(queryDesc))))
#define QdGetFeature(queryDesc)      (List) CAR(CDR(CDR(CDR(CDR(queryDesc)))))
#define QdGetDest(queryDesc) \
    (CommandDest) CInteger(CAR(CDR(CDR(CDR(CDR(CDR(queryDesc)))))))

#define QdSetState(queryDesc, s) \
    (CAR(CDR(CDR(CDR(queryDesc)))) = (List) s)

#define QdSetFeature(queryDesc, f) \
    (CAR(CDR(CDR(CDR(CDR(queryDesc))))) = (List) f)

#define QdSetDest(queryDesc, d) \
    (((List) CAR(CDR(CDR(CDR(CDR(CDR(queryDesc)))))))->val.fixnum = d)

/* ----------------
 *	query feature accessors
 * ----------------
 */
#define FeatureGetCommand(feature)	CAR(feature)
#define FeatureGetCount(feature)	CAR(CDR(feature))

#define QdGetCommand(queryDesc)	FeatureGetCommand(QdGetFeature(queryDesc))
#define QdGetCount(queryDesc)	FeatureGetCount(QdGetFeature(queryDesc))

/* ----------------
 *	parse tree accessors
 * ----------------
 */
#define parse_tree_root(parse_tree) \
    CAR(parse_tree)

#define parse_tree_target_list(parse_tree) \
    CAR(CDR(parse_tree))

#define parse_tree_qualification(parse_tree) \
    CAR(CDR(CDR(parse_tree)))

#define parse_tree_root_num_levels(parse_tree_root) \
    CAR(parse_tree_root)

#define parse_tree_root_command_type(parse_tree_root) \
    CAR(CDR(parse_tree_root))

#define parse_tree_root_result_relation(parse_tree_root) \
    CAR(CDR(CDR(parse_tree_root)))

#define parse_tree_root_range_table(parse_tree_root) \
    CAR(CDR(CDR(CDR(parse_tree_root))))

#define parse_tree_root_priority(parse_tree_root) \
    CAR(CDR(CDR(CDR(CDR(parse_tree_root)))))

#define parse_tree_root_rule_info(parse_tree_root) \
    CAR(CDR(CDR(CDR(CDR(CDR((parse_tree_root)))))))


#define parse_tree_range_table(parse_tree)	\
    parse_tree_root_range_table(parse_tree_root(parse_tree))

#define parse_tree_result_relation(parse_tree) \
    parse_tree_root_result_relation(parse_tree_root(parse_tree))

#endif  ExecdescHIncluded
