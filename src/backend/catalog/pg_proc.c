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
#include "catalog/pg_protos.h"
#include "catalog/indexing.h"
#include "parser/parse.h"  /* temporary */
#include "tcop/dest.h"

/* ----------------------------------------------------------------
 *	ProcedureDefine
 * ----------------------------------------------------------------
 */

/*ARGSUSED*/
void
ProcedureDefine(procedureName, returnsSet, returnTypeName, languageName,
		prosrc, probin, canCache, trusted, byte_pct, perbyte_cpu, 
		percall_cpu, outin_ratio, argList, dest)
     Name 		procedureName;
     bool		returnsSet;
     Name 		returnTypeName;	
     Name 		languageName;
     char 		*prosrc;
     char               *probin;
     Boolean		canCache;
     Boolean            trusted;
     int32              byte_pct, perbyte_cpu, percall_cpu, outin_ratio;
     List		argList;
     CommandDest	dest;
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
    List		parsetree_list;
    List		plan_list;
    ObjectId		typev[8];
    static char oid_string[64];
    static char temp[8];
    ObjectId relid;
    List t;
    ObjectId toid;

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

    /* don't allow functions of complex types that have the same name as
       existing attributes of the type */
    if (parameterCount == 1 && 
	(toid = TypeGet((Name)(CString(CAR(argList))), &defined)) &&
	defined &&
	(relid = typeid_get_relid(toid)) != 0 &&
	get_attnum(relid, procedureName) != InvalidAttributeNumber)
      elog(WARN, "method %s already an attribute of type %s",
	   procedureName, CString(CAR(argList)));

    parameterCount = 0;
    oid_string[0] = '\0';
    foreach (x, argList) {
	t = CAR(x);

	if (parameterCount == 8)
	    elog(WARN, "Procedures cannot take more than 8 arguments");

	toid = TypeGet((Name)(CString(t)), &defined);

	if (!ObjectIdIsValid(toid)) {
	    elog(WARN, "ProcedureDefine: arg type '%s' is not defined",
		 CString(t));
	}

	typev[parameterCount++] = toid;

	sprintf(temp, "%ld ", toid);
	strcat(oid_string, temp);
    }

    /*
     *  If this is a postquel procedure, we parse it here in order to
     *  be sure that it contains no syntax errors.  We should store
     *  the plan in an Inversion file for use later, but for now, we
     *  just store the procedure's text in the prosrc attribute.
     */

    if (strncmp(languageName, "postquel", 16) == 0) {
	plan_list = (List) pg_plan(prosrc, typev, parameterCount,
				   &parsetree_list, dest);

	/* typecheck return value */
	pg_checkretval(typeObjectId, parsetree_list);
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

    values[i++] = (char *) trusted;
    values[i++] = (char *) canCache;
    values[i++] = (char *) parameterCount;
    values[i++] = (char *) returnsSet;
    values[i++] = (char *) typeObjectId;

    values[i++] = fmgr(F_OID8IN, oid_string);
    /*
     * The following assignments of constants are made.  The real values
     * will have to be extracted from the arglist someday soon.
     */
    values[i++] = (char *) byte_pct; /* probyte_pct */
    values[i++] = (char *) perbyte_cpu; /* properbyte_cpu */
    values[i++] = (char *) percall_cpu; /* propercall_cpu */
    values[i++] = (char *) outin_ratio; /* prooutin_ratio */

    values[i++] = fmgr(TextInRegProcedure, prosrc);	/* prosrc */
    values[i++] = fmgr(TextInRegProcedure, probin);   /* probin */

    rdesc = RelationNameOpenHeapRelation(ProcedureRelationName);
    tup = FormHeapTuple(ProcedureRelationNumberOfAttributes,
			&rdesc->rd_att,
			values,
			nulls);

    RelationInsertHeapTuple(rdesc, (HeapTuple) tup, (double *) NULL);

    if (RelationGetRelationTupleForm(rdesc)->relhasindex)
    {
	Relation idescs[Num_pg_proc_indices];

	CatalogOpenIndices(Num_pg_proc_indices, Name_pg_proc_indices, idescs);
	CatalogIndexInsert(idescs, Num_pg_proc_indices, rdesc, tup);
	CatalogCloseIndices(Num_pg_proc_indices, idescs);
    }
    RelationCloseHeapRelation(rdesc);
}
