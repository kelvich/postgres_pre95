/* ----------------------------------------------------------------
 *   FILE
 *	pg_proc.c
 *	
 *   DESCRIPTION
 *	routines to support manipulation of the pg_proc relation
 *
 *   INTERFACE ROUTINES
 *
 *   NOTES
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#include "c.h"
RcsId("$Header$");

/* ----------------
 *	these blindly taken from commands/define.c
 *	XXX clean these up!
 * ----------------
 */
#include <strings.h>
#include "cat.h"

#include "anum.h"
#include "catname.h"
#include "fmgr.h"
#include "ftup.h"
#include "heapam.h"
#include "htup.h"
#include "log.h"
#include "name.h"
#include "parse.h"
#include "pg_lisp.h"
#include "rproc.h"
#include "syscache.h"
#include "tqual.h"

#include "defrem.h"

/* ----------------
 *	support functions in pg_type.c
 * ----------------
 */
extern ObjectId	TypeGet();
extern ObjectId	TypeShellMake();

/* ----------------------------------------------------------------
 *	ProcedureDefine
 * ----------------------------------------------------------------
 */
/*#define USEPARGS */	/* XXX */

/*ARGSUSED*/
void
ProcedureDefine(procedureName, returnTypeName, languageName, fileName,
		canCache, argList)
    Name 		procedureName;	
    Name 		returnTypeName;	
    Name 		languageName;	
    char 		*fileName;	/* XXX path to binary */
    Boolean		canCache;
    List		argList;
{
    register		i;
    Relation 		rdesc;
    HeapTuple 		tup;
    bool              	defined;
    uint16 		parameterCount;
    char		nulls[ ProcedureRelationNumberOfAttributes ];
    char 		*values[ ProcedureRelationNumberOfAttributes ];
    ObjectId 		languageObjectId;
    ObjectId		typeObjectId;
#ifdef USEPARGS
    ObjectId		procedureObjectId;
#endif
    
    /* ----------------
     *	sanity checks
     * ----------------
     */
    Assert(NameIsValid(procedureName));
    Assert(NameIsValid(returnTypeName));
    Assert(NameIsValid(languageName));
    Assert(PointerIsValid(fileName));

    /* ----------------
     *	
     * ----------------
     */
    parameterCount = length(argList);

    /* ----------------
     *	
     * ----------------
     */
    tup = SearchSysCacheTuple(PRONAME,
			      (char *) procedureName,
			      (char *) NULL,
			      (char *) NULL,
			      (char *) NULL);

    if (HeapTupleIsValid(tup))
	elog(WARN, "ProcedureDefine: procedure %s already exists",
	     procedureName);

    /* ----------------
     *	
     * ----------------
     */
    tup = SearchSysCacheTuple(LANNAME,
			      (char *) languageName,
			      (char *) NULL,
			      (char *) NULL,
			      (char *) NULL);

    if (!HeapTupleIsValid(tup))
	elog(WARN, "ProcedureDefine: no such language %s",
	     languageName);

    languageObjectId = tup->t_oid;
    
    /* ----------------
     *	
     * ----------------
     */
    typeObjectId = TypeGet(returnTypeName, &defined);
    if (!ObjectIdIsValid(typeObjectId)) {
	elog(NOTICE, "ProcedureDefine: return type %s not yet defined",
	     returnTypeName);

	typeObjectId = TypeShellMake(returnTypeName);
	if (!ObjectIdIsValid(typeObjectId))
	    elog(WARN, "ProcedureDefine: could not create type %s",
		 returnTypeName);
    }
    
    /* ----------------
     *	
     * ----------------
     */
    for (i = 0; i < ProcedureRelationNumberOfAttributes; ++i) {
	nulls[i] = ' ';
	values[i] = (char *) NULL;
    }

    /* ----------------
     *	
     * ----------------
     */
    i = 0;
    values[i++] = (char *) procedureName;
    values[i++] = (char *) (ObjectId) getuid();
    values[i++] = (char *) languageObjectId;

    /* XXX isinherited is always false for now */

    values[i++] = (char *) (Boolean) 0;

    /* XXX istrusted is always false for now */

    values[i++] = (char *) (Boolean) 0;
    values[i++] = (char *) canCache;
    values[i++] = (char *) parameterCount;
    values[i++] = (char *) typeObjectId;

    /* ----------------
     *	
     * ----------------
     */
    /*  XXX Fix this when prosrc is used */
    values[i++] = fmgr(TextInRegProcedure, "-");	/* prosrc */
    values[i++] = fmgr(TextInRegProcedure, fileName); /* probin */

    /* ----------------
     *	
     * ----------------
     */
    rdesc = RelationNameOpenHeapRelation(ProcedureRelationName);
    tup = FormHeapTuple(ProcedureRelationNumberOfAttributes,
			&rdesc->rd_att,
			values,
			nulls);
    
    RelationInsertHeapTuple(rdesc, (HeapTuple) tup, (double *) NULL);
#ifdef USEPARGS
    procedureObjectId = tup->t_oid;
#endif
    RelationCloseHeapRelation(rdesc);

    /* XXX Test to see if tuple inserted ?? */
    
    /* ----------------
     *	
     * ----------------
     */
    /* XXX don't fill in PARG for now (not used for anything) */

#ifdef USEPARGS
    rdesc = RelationNameOpenHeapRelation(ProcedureArgumentRelationName);
    for (i = 0; i < parameterCount; ++i) {
	HeapTuple	typeTuple;

	typeTuple = SearchSysCacheTuple(TYPNAME,
					CString(CAR(argList))
					(char *) NULL,
					(char *) NULL,
					(char *) NULL);
	
	if (!HeapTupleIsValid(typeTuple)) {
	    elog(WARN, "DEFINE FUNCTION: arg type \"%s\" unknown",
		 CString(CAR(argList)));
	}

	j = 0;
	values[j++] = (char *) procedureObjectId;
	values[j++] = (char *)(1 + i);
	/* XXX ignore array bound for now */
	values[j++] = (char *) '\0';
	values[j++] = (char *)typeTuple->t_oid;
	tup = FormHeapTuple(ProcedureArgumentRelationNumberOfAttributes,
			    &rdesc->rd_att,
			    values,
			    nulls);

	RelationInsertHeapTuple(rdesc, (HeapTuple) tup, (double *) NULL);
    }
    
    RelationCloseHeapRelation(rdesc);
#endif /* USEPARGS */
    
}
