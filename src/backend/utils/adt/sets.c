/*
 * sets.c --
 *       Functions for sets, which are defined by queries.
 *       Example:   a set is defined as being the result of the query
 *           retrieve (X.all)
 *
 *  $Header$
 */

#include "tmp/postgres.h"
#include "tcop/dest.h"
#include "utils/log.h"
#include "rules/prs2.h"
#include "nodes/pg_lisp.h"          /* for LispValue and List */
#include "access/htup.h"            /* for HeapTuple */
#include "catalog/pg_proc.h"        /* for Form_pg_proc */
#include "catalog/syscache.h"       /* for PROOID */
#include "catalog/catname.h"        /* for ProcedureRelationName */
#include "catalog/indexing.h"       /* for Num_pg_proc_indices */
#include "utils/sets.h"             /* for GENERICSETNAME      */

extern CommandDest whereToSendOutput;  /* defined in tcop/postgres.c */


/*
 *    SetDefine        - converts query string defining set to an oid
 *
 *    The query string is used to store the set as a function in 
 *    pg_proc.  The name of the function is then changed to use the
 *    OID of its tuple in pg_proc.
 */
ObjectId
     SetDefine(querystr, typename)
char *querystr;
Name typename;
{
     ObjectId setoid;
     Name procname = (Name)GENERICSETNAME;
     char *fileName = "-";
     char realprocname[16];
     HeapTuple tup, newtup;
     Form_pg_proc proc;
     Relation procrel;
     int i;
     Datum replValue[Natts_pg_proc];
     char replNull[Natts_pg_proc];
     char repl[Natts_pg_proc];
     HeapScanDesc pg_proc_scan;
     Buffer buffer;
     ItemPointerData ipdata;
     
     static ScanKeyEntryData	oidKey[1] = {
     { 0, ObjectIdAttributeNumber, ObjectIdEqualRegProcedure }};
     
     setoid = ProcedureDefine(procname, /* changed below, after oid known */
			      true,               /* returnsSet */
			      (String)typename,   /* returnTypeName */
			      "postquel",         /* languageName */
			      querystr,           /* sourceCode */
			      fileName,           /* fileName */
			      false,              /* canCache */
			      true,               /* trusted */
			      100,                /* byte_pct */ 
			      0,                  /* perbyte_cpu */
			      0,                  /* percall_cpu */
			      100,                /* outin_ratio */
			      LispNil,            /* argList */
			      whereToSendOutput);
     /* Since we're still inside this command of the transaction, we can't
      * see the results of the procedure definition unless we pretend
      * we've started the next command.  (Postgres's solution to the
      * Halloween problem is to not allow you to see the results of your
      * command until you start the next command.)
      */
     CommandCounterIncrement();
     tup = SearchSysCacheTuple(PROOID,
			       setoid,
			       (char *) NULL,
			       (char *) NULL,
			       (char *) NULL);
     if (!HeapTupleIsValid(tup))
	  elog(WARN, "setin: unable to define set %s", querystr);
     
     /* We can tell whether the set was already defined by checking
      * the name.   If it's GENERICSETNAME, the set is new.  If it's
      * "set<some oid>" it's already defined.
      */
     proc = (Form_pg_proc)GETSTRUCT(tup);
     if (!strcmp((char*)procname, (char*)&(proc->proname))) {
	  /* make the real proc name */
	  sprintf(realprocname, "set%u", setoid);

	  /* set up the attributes to be modified or kept the same */
	  repl[0] = 'r';
	  for (i = 1; i < Natts_pg_proc; i++) repl[i] = ' ';
	  replValue[0] = (Datum)realprocname;
	  for (i = 1; i < Natts_pg_proc; i++) replValue[i] = (Datum)0;
	  for (i = 0; i < Natts_pg_proc; i++) replNull[i] = ' ';

	  /* change the pg_proc tuple */
	  procrel = RelationNameOpenHeapRelation(ProcedureRelationName);
	  RelationSetLockForWrite(procrel);
	  fmgr_info(ObjectIdEqualRegProcedure, 
		    &oidKey[0].func, 
		    &oidKey[0].nargs);
	  oidKey[0].argument = ObjectIdGetDatum(setoid);
	  pg_proc_scan = heap_beginscan(procrel,
					0,
					SelfTimeQual,
					1,
					(ScanKey) oidKey);
	  tup = heap_getnext(pg_proc_scan, 0, &buffer);
	  if (HeapTupleIsValid(tup)) {
	       newtup = heap_modifytuple(tup, 
				buffer, 
				procrel, 
				replValue, 
				replNull, 
				repl);
	       
	       /* XXX may not be necessary */
	       ItemPointerCopy(&tup->t_ctid, &ipdata);
	       
	       setheapoverride(true);
	       heap_replace(procrel, &ipdata, newtup);
	       setheapoverride(false);
	       
	       setoid = newtup->t_oid;
	  } else
	       elog(WARN, "setin: could not find new set oid tuple");
	  heap_endscan(pg_proc_scan);
	  
	  if (RelationGetRelationTupleForm(procrel)->relhasindex)
	  {
	       Relation idescs[Num_pg_proc_indices];
	       
	       CatalogOpenIndices(Num_pg_proc_indices, Name_pg_proc_indices, idescs);
	       CatalogIndexInsert(idescs, Num_pg_proc_indices, procrel, newtup);
	       CatalogCloseIndices(Num_pg_proc_indices, idescs);
	  }
	  RelationUnsetLockForWrite(procrel);
	  heap_close(procrel);
     }
     return setoid;
}

/* This function is a placeholder.  The parser uses the OID of this
 * function to fill in the :funcid field  of a set.  This routine is
 * never executed.  At runtime, the OID of the actual set is substituted
 * into the :funcid.
 */
int
seteval(funcoid)
ObjectId funcoid;
{
     return 17;
}
