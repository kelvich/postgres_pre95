/* ----------------------------------------------------------------
 *   FILE
 *     	execdesc.h
 *     
 *   DESCRIPTION
 *     	plan and query descriptor accessor macros used
 *      by the executor and related modules.
 *
 *   NOTES
 *	parse tree macros moved to H/parser/parsetree.h
 *
 *   IDENTIFICATION
 *	$Header$"
 * ----------------------------------------------------------------
 */

#ifndef ExecdescHIncluded
#define ExecdescHIncluded 1	/* include only once */

/* ----------------
 *	query descriptor macros
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


#endif  ExecdescHIncluded
