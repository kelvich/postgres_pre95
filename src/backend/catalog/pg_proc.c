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

#include "tmp/postgres.h"

 RcsId("$Header$");

#include "access/ftup.h"
#include "access/heapam.h"
#include "access/relscan.h"
#include "access/skey.h"
#include "utils/rel.h"
#include "utils/fmgr.h"
#include "utils/log.h"

#include "nodes/pg_lisp.h"

#include "catalog/catname.h"
#include "catalog/syscache.h"
#include "catalog/pg_proc.h"
#include "parser/parse.h"  /* temporary */
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
ProcedureDefine(procedureName, returnTypeName, languageName, prosrc, probin,
		canCache, argList)
     Name 		procedureName;
     Name 		returnTypeName;	
     Name 		languageName;
     char 		*prosrc;
     char               *probin;
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
    List x;
    static char oid_string[64];
    static char temp[8];
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
    Assert(PointerIsValid(prosrc));
    Assert(PointerIsValid(probin));

    parameterCount = length(argList);

    if (parameterCount > 8)	/* until oid10 */
	elog(WARN, "A function may have only 8 arguments"); 

    tup = SearchSysCacheTuple(PRONAME,
			      (char *) procedureName,
			      (char *) NULL,
			      (char *) NULL,
			      (char *) NULL);

    if (HeapTupleIsValid(tup))
	elog(WARN, "ProcedureDefine: procedure %s already exists",
	     procedureName);

    tup = SearchSysCacheTuple(LANNAME,
			      (char *) languageName,
			      (char *) NULL,
			      (char *) NULL,
			      (char *) NULL);

    if (!HeapTupleIsValid(tup))
	elog(WARN, "ProcedureDefine: no such language %s",
	     languageName);

    languageObjectId = tup->t_oid;

    typeObjectId = TypeGet(returnTypeName, &defined);
    if (!ObjectIdIsValid(typeObjectId)) {
	elog(NOTICE, "ProcedureDefine: return type '%s' not yet defined",
	     returnTypeName);

	typeObjectId = TypeShellMake(returnTypeName);
	if (!ObjectIdIsValid(typeObjectId))
	    elog(WARN, "ProcedureDefine: could not create type '%s'",
		 returnTypeName);
    }
    oid_string[0] = '\0';
    foreach (x, argList) {
	List t = CAR(x);
	ObjectId toid;
	
	
	if (!strcmp(CString(t), "RELATION")) {
	    toid = RELATION;
	}
	else toid = TypeGet(CString(t), &defined);
	if (!ObjectIdIsValid(toid)) {
	    elog(WARN, "ProcedureDefine: arg type '%s' is not defined",
		 CString(t));
	}
	sprintf(temp, "%ld ", toid);
	strcat(oid_string, temp);
    }

    for (i = 0; i < ProcedureRelationNumberOfAttributes; ++i) {
	nulls[i] = ' ';
	values[i] = (char *) NULL;
    }

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

    values[i++] = fmgr(F_OID8IN, oid_string);
    values[i++] = fmgr(TextInRegProcedure, prosrc);	/* prosrc */
    values[i++] = fmgr(TextInRegProcedure, probin);   /* probin */

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
