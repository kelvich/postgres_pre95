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
#include "fmgr.h"
#include "utils/log.h"
#include "utils/builtins.h"
#include "utils/sets.h"

#include "nodes/pg_lisp.h"

#include "catalog/catname.h"
#include "catalog/syscache.h"
#include "catalog/pg_proc.h"
#include "catalog/pg_protos.h"
#include "catalog/indexing.h"
#include "parse.h"  		/* temporary */
#include "tcop/dest.h"
#include "tcop/tcopprot.h"

/* ----------------------------------------------------------------
 *	ProcedureDefine
 * ----------------------------------------------------------------
 */

/*ARGSUSED*/
ObjectId
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
    static char temp[8];
    ObjectId relid;
    List t;
    ObjectId toid;
    text *prosrctext;

    /* ----------------
     *	sanity checks
     * ----------------
     */
    Assert(NameIsValid(procedureName));
    Assert(NameIsValid(returnTypeName));
    Assert(NameIsValid(languageName));
    Assert(PointerIsValid(prosrc));
    Assert(PointerIsValid(probin));

    parameterCount = 0;
    bzero(typev, 8 * sizeof(ObjectId));
    foreach (x, argList) {
	t = CAR(x);

	if (parameterCount == 8)
	    elog(WARN, "Procedures cannot take more than 8 arguments");

	if (strcmp(CString(t), "any") == 0) {
	    if (namestrcmp(languageName, "postquel") == 0) {
		elog(WARN, "ProcedureDefine: postquel functions cannot take type \"any\"");
	    }
	    else
		toid = 0;
	}

	else {
	    toid = TypeGet((Name)(CString(t)), &defined);

	    if (!ObjectIdIsValid(toid)) {
		elog(WARN, "ProcedureDefine: arg type '%s' is not defined",
		     CString(t));
	    }

	    if (!defined) {
		elog(NOTICE, "ProcedureDefine: arg type '%s' is only a shell",
		     CString(t));
	    }
	}

	typev[parameterCount++] = toid;

    }

    tup = SearchSysCacheTuple(PRONAME,
			      (char *) procedureName,
			      (char *) UInt16GetDatum(parameterCount),
			      (char *) typev,
			      (char *) NULL);

    if (HeapTupleIsValid(tup))
	elog(WARN, "ProcedureDefine: procedure %s already exists with same arguments",
	     procedureName);

    if (!namestrcmp((char *)languageName, "postquel"))  {
	 /* If this call is defining a set, check if the set is already
	  * defined by looking to see whether this call's function text
	  * matches a function already in pg_proc.  If so just return the 
	  * OID of the existing set.
	  */
	 if (!namestrcmp((char*)procedureName, GENERICSETNAME)) {
	      prosrctext = textin(prosrc);
	      tup = SearchSysCacheTuple(PROSRC,
					(char *) prosrctext,
					(char *) NULL,
					(char *) NULL,
					(char *) NULL);
	      if (HeapTupleIsValid(tup))
		   return tup->t_oid;
	 }
    }

    tup = SearchSysCacheTuple(LANNAME,
			      (char *) languageName,
			      (char *) NULL,
			      (char *) NULL,
			      (char *) NULL);

    if (!HeapTupleIsValid(tup))
	elog(WARN, "ProcedureDefine: no such language %s",
	     languageName);

    languageObjectId = tup->t_oid;

    if (namestrcmp(returnTypeName, "any") == 0) {
	if (namestrcmp(languageName, "postquel") == 0) {
	    elog(WARN, "ProcedureDefine: postquel functions cannot return type \"any\"");
	}
	else
	    typeObjectId = 0;
    }

    else {
	typeObjectId = TypeGet(returnTypeName, &defined);

	if (!ObjectIdIsValid(typeObjectId)) {
	    elog(NOTICE, "ProcedureDefine: type '%s' is not yet defined",
		 returnTypeName);
	    elog(NOTICE, "ProcedureDefine: creating a shell for type '%s'",
		 returnTypeName);

	    typeObjectId = TypeShellMake(returnTypeName);
	    if (!ObjectIdIsValid(typeObjectId)) {
		elog(WARN, "ProcedureDefine: could not create type '%s'",
		     returnTypeName);
	    }
	}

	else if (!defined) {
	    elog(NOTICE, "ProcedureDefine: return type '%s' is only a shell",
		 returnTypeName);
	}
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


    /*
     *  If this is a postquel procedure, we parse it here in order to
     *  be sure that it contains no syntax errors.  We should store
     *  the plan in an Inversion file for use later, but for now, we
     *  just store the procedure's text in the prosrc attribute.
     */

    if (namestrcmp(languageName, "postquel") == 0) {
	plan_list = pg_plan(prosrc, typev, parameterCount,
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
    values[i++] = (char *) Int32GetDatum(GetUserId());
    values[i++] = (char *) ObjectIdGetDatum(languageObjectId);

    /* XXX isinherited is always false for now */

    values[i++] = (char *) Int8GetDatum((Boolean) 0);

    /* XXX istrusted is always false for now */

    values[i++] = (char *) Int8GetDatum(trusted);
    values[i++] = (char *) Int8GetDatum(canCache);
    values[i++] = (char *) UInt16GetDatum(parameterCount);
    values[i++] = (char *) Int8GetDatum(returnsSet);
    values[i++] = (char *) ObjectIdGetDatum(typeObjectId);

    values[i++] = (char *) typev;
    /*
     * The following assignments of constants are made.  The real values
     * will have to be extracted from the arglist someday soon.
     */
    values[i++] = (char *) Int32GetDatum(byte_pct); /* probyte_pct */
    values[i++] = (char *) Int32GetDatum(perbyte_cpu); /* properbyte_cpu */
    values[i++] = (char *) Int32GetDatum(percall_cpu); /* propercall_cpu */
    values[i++] = (char *) Int32GetDatum(outin_ratio); /* prooutin_ratio */

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
    return tup->t_oid;
}



