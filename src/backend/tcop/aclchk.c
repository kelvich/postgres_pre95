/* ----------------------------------------------------------------
 *   FILE
 *     	aclchk.c
 *
 *   DESCRIPTION
 *     	Routines to check access control permissions.
 *
 *   NOTES
 *	See acl.h.
 *
 *   IDENTIFICATION
 *   	$Header$
 * ----------------------------------------------------------------
 */

#include "tmp/acl.h"
#include "access/heapam.h"
#include "access/htup.h"
#include "access/tupmacs.h"
#include "utils/log.h"
#include "utils/palloc.h"
#include "catalog/pg_group.h"
#include "catalog/pg_proc.h"
#include "catalog/pg_relation.h"
#include "catalog/pg_user.h"
#include "catalog/syscache.h"

/*
 * Enable use of user relations in place of real system catalogs.
 */
#define ACLDEBUG

#ifdef ACLDEBUG
/* Fool the code below into thinking that "pgacls" is pg_class.
 * relname and relowner are in the same place, happily.
 */
#undef	Anum_pg_relation_relacl
#define	Anum_pg_relation_relacl		3
#undef	Natts_pg_relation
#define	Natts_pg_relation		3
#undef	Name_pg_relation
#define	Name_pg_relation		"pgacls"
#undef	Name_pg_group
#define	Name_pg_group			"pggroup"
#endif

#ifdef ACLDEBUG_TRACE
static
dumpacl(acl)
	Acl *acl;
{
	register unsigned i;
	AclItem *aip;
	
	elog(DEBUG, "acl size = %d, # acls = %d",
	     VARSIZE(acl), ACL_NUM(acl));
	aip = (AclItem *) ACL_DAT(acl);
	for (i = 0; i < ACL_NUM(acl); ++i)
		elog(DEBUG, "	acl[%d]: %s", i, aclitemout(aip + i));
}
#endif

/*
 * 
 */
ChangeAcl(relname, mod_aip, modechg)
	char *relname; /* string, not Name */
	AclItem *mod_aip;
	unsigned modechg;
{
	register unsigned i;
	Acl *old_acl = (Acl *) NULL, *new_acl;
	Relation relation;
	static ScanKeyEntryData relkey[1] = {
		{ 0, Anum_pg_relation_relname, NameEqualRegProcedure }
	};
	HeapScanDesc hsdp;
	HeapTuple htp;
	Buffer buffer;
	char *values[Natts_pg_relation];
	char nulls[Natts_pg_relation];
	char replaces[Natts_pg_relation];
	ItemPointerData tmp_ipd;

	/* Find the pg_class tuple matching 'relname' and extract the ACL.
	 * If there's no ACL, create a default using the pg_class.relowner
	 * field.
	 *
	 * We can't use the syscache here, since we need to do a heap_replace
	 * on the tuple we find.  Feh.
	 */
	relation = heap_openr(Name_pg_relation);
	if (!RelationIsValid(relation))
		elog(WARN, "ChangeAcl: could not open \"%s\"??",
		     Name_pg_relation);
	fmgr_info(NameEqualRegProcedure, &relkey[0].func, &relkey[0].nargs);
	relkey[0].argument = NameGetDatum(relname);
	hsdp = heap_beginscan(relation,
			      0,
			      NowTimeQual,
			      (unsigned) 1,
			      (ScanKey) relkey);
	htp = heap_getnext(hsdp, 0, &buffer);
	if (!HeapTupleIsValid(htp)) {
		heap_endscan(hsdp);
		heap_close(relation);
		elog(WARN, "ChangeAcl: class \"%s\" not found", relname);
		return;
	}
	if (!heap_attisnull(htp, Anum_pg_relation_relacl))
		old_acl = (Acl *) heap_getattr(htp,
					       &buffer,
					       Anum_pg_relation_relacl,
					       &relation->rd_att,
					       (bool *) NULL);
	if (!old_acl || ACL_NUM(old_acl) < 1) {
#ifdef ACLDEBUG_TRACE
		elog(DEBUG, "ChangeAcl: using default ACL");
#endif
		old_acl = acldefault(((Form_pg_relation)
				      GETSTRUCT(htp))->relowner);
	}

#ifdef ACLDEBUG_TRACE
	dumpacl(old_acl);
#endif
	new_acl = aclinsert3(old_acl, mod_aip, modechg);
#ifdef ACLDEBUG_TRACE
	dumpacl(new_acl);
#endif

	for (i = 0; i < Natts_pg_relation; ++i) {
		replaces[i] = ' ';
		nulls[i] = ' ';	  /* ignored if replaces[i] == ' ' anyway */
		values[i] = NULL; /* ignored if replaces[i] == ' ' anyway */
	}
	replaces[Anum_pg_relation_relacl - 1] = 'r';
	values[Anum_pg_relation_relacl - 1] = (char *) new_acl;
	htp = heap_modifytuple(htp,
			       buffer,
			       relation,
			       values,
			       nulls,
			       replaces);
	/* XXX is this necessary? */
	ItemPointerCopy(&htp->t_ctid, &tmp_ipd);
	/* XXX handle index on pg_class? */
	setheapoverride(true);
	(void) heap_replace(relation, &tmp_ipd, htp);
	setheapoverride(false);
	heap_endscan(hsdp);
	heap_close(relation);
	pfree(new_acl);
}

AclId
get_grosysid(groname)
	char *groname;
{
#ifndef ACLDEBUG
	/* XXX syscache */
	return(0);
#else
	Relation relation;
	static ScanKeyEntryData groupkey[1] = {
		{ 0, Anum_pg_group_groname, NameEqualRegProcedure }
	};
	HeapScanDesc hsdp;
	HeapTuple htp;
	struct varlena *tmp;
	AclId id = 0;

	relation = heap_openr(Name_pg_group);
	if (!RelationIsValid(relation)) {
		elog(NOTICE, "in_group: could not open \"%s\"??",
		     Name_pg_group);
		return(0);
	}
	fmgr_info(NameEqualRegProcedure,
		  &groupkey[0].func,
		  &groupkey[0].nargs);
	groupkey[0].argument = NameGetDatum(groname);
	hsdp = heap_beginscan(relation,
			      0,
			      NowTimeQual,
			      1,
			      (ScanKey) groupkey);
	htp = heap_getnext(hsdp, 0, (Buffer *) 0);
	if (HeapTupleIsValid(htp))
		id = ((Form_pg_group) GETSTRUCT(htp))->grosysid;
	heap_endscan(hsdp);
	heap_close(relation);
	return(id);
#endif
}

char *
get_groname(grosysid)
	AclId grosysid;
{
#ifndef ACLDEBUG
	/* XXX syscache */
	return(0);
#else
	Relation relation;
	static ScanKeyEntryData groupkey[1] = {
		{ 0, Anum_pg_group_grosysid, Integer16EqualRegProcedure }
	};
	HeapScanDesc hsdp;
	HeapTuple htp;
	struct varlena *tmp;
	char *name;

	relation = heap_openr(Name_pg_group);
	if (!RelationIsValid(relation)) {
		elog(NOTICE, "get_groname: could not open \"%s\"??",
		     Name_pg_group);
		return(0);
	}
	fmgr_info(Integer16EqualRegProcedure,
		  &groupkey[0].func,
		  &groupkey[0].nargs);
	groupkey[0].argument = UInt16GetDatum(grosysid);
	hsdp = heap_beginscan(relation,
			      0,
			      NowTimeQual,
			      1,
			      (ScanKey) groupkey);
	htp = heap_getnext(hsdp, 0, (Buffer *) 0);
	if (HeapTupleIsValid(htp))
		name = (char *) &((Form_pg_group) GETSTRUCT(htp))->groname;
	heap_endscan(hsdp);
	heap_close(relation);
	return(name);
#endif
}

static
int32
in_group(uid, gid)
	AclId uid, gid;
{
#ifndef ACLDEBUG
	/* XXX syscache */
	return(0);
#else
	Relation relation;
	static ScanKeyEntryData groupkey[1] = {
		{ 0, Anum_pg_group_grosysid, Integer16EqualRegProcedure }
	};
	HeapScanDesc hsdp;
	HeapTuple htp;
	struct varlena *tmp;
	unsigned i, num;
	AclId *aidp;
	int32 found = 0;

	relation = heap_openr(Name_pg_group);
	if (!RelationIsValid(relation)) {
		elog(NOTICE, "in_group: could not open \"%s\"??",
		     Name_pg_group);
		return(0);
	}
	fmgr_info(Integer16EqualRegProcedure,
		  &groupkey[0].func,
		  &groupkey[0].nargs);
	groupkey[0].argument = UInt16GetDatum(gid);
	hsdp = heap_beginscan(relation,
			      0,
			      NowTimeQual,
			      1,
			      (ScanKey) groupkey);
	htp = heap_getnext(hsdp, 0, (Buffer *) 0);
	if (HeapTupleIsValid(htp) &&
	    !heap_attisnull(htp, Anum_pg_group_grolist)) {
		tmp = (IdList *) heap_getattr(htp,
					      (Buffer) 0,
					      Anum_pg_group_grolist,
					      &relation->rd_att,
					      (bool *) NULL);
		/* XXX make me a function */
		num = IDLIST_NUM(tmp);
		aidp = IDLIST_DAT(tmp);
		for (i = 0; i < num; ++i)
			if (aidp[i] == uid) {
				found = 1;
				break;
			}
	}
	heap_endscan(hsdp);
	heap_close(relation);
	return(found);
#endif
}

/*
 * aclcheck
 * Returns 1 if the 'id' of type 'idtype' has ACL entries in 'acl' to satisfy
 * any one of the requirements of 'mode'.  Returns 0 otherwise.
 */
int32
aclcheck(acl, id, idtype, mode)
	Acl *acl;
	AclId id;
	AclIdType idtype;
	AclMode mode;
{
	register unsigned i;
	register AclItem *aip, *aidat;
	unsigned num, found_group;

	/* For backward-compatibility with old catalogs, a null 'acl'
	 * is assumed to mean "=arwR", which always succeeds.
	 */
	if (!acl) {
		elog(DEBUG, "aclcheck: null ACL, returning 1");
		return(1);
	}

	num = ACL_NUM(acl);
	aidat = ACL_DAT(acl);

	/* We'll treat the empty ACL like that, too, although this is more
	 * like an error (i.e., you manually blew away your ACL array) --
	 * the system never creates an empty ACL.
	 */
	if (num < 1) {
#ifdef ACLDEBUG_TRACE
		elog(DEBUG, "aclcheck: zero-length ACL, returning 1");
#endif
		return(1);
	}

	switch (idtype) {
	case ACL_IDTYPE_UID:
		for (i = 1, aip = aidat + 1; /* skip world entry */
		     i < num && aip->ai_idtype == ACL_IDTYPE_UID;
		     ++i, ++aip) {
			if (aip->ai_id == id) {
#ifdef ACLDEBUG_TRACE
				elog(DEBUG, "aclcheck: found %d/%d",
				     aip->ai_id, aip->ai_mode);
#endif
				return((aip->ai_mode & mode) ? 1 : 0);
			}
		}
		for (found_group = 0;
		     i < num && aip->ai_idtype == ACL_IDTYPE_GID;
		     ++i, ++aip) {
			if (in_group(id, aip->ai_id)) {
				if (aip->ai_mode & mode)
					++found_group;
				else {
#ifdef ACLDEBUG_TRACE
					elog(DEBUG, "aclcheck: found %d/%d",
					     aip->ai_id, aip->ai_mode);
#endif
					return(0);
				}
			}
		}
		if (found_group) {
#ifdef ACLDEBUG_TRACE
			elog(DEBUG,"aclcheck: all groups ok");
#endif
			return(1);
		}
		break;
	case ACL_IDTYPE_GID:
		for (i = 1, aip = aidat + 1; /* skip world entry and UIDs */
		     i < num && aip->ai_idtype == ACL_IDTYPE_UID;
		     ++i, ++aip)
			;
		for (;
		     i < num && aip->ai_idtype == ACL_IDTYPE_GID;
		     ++i, ++aip) {
			if (aip->ai_id == id) {
#ifdef ACLDEBUG_TRACE
				elog(DEBUG, "aclcheck: found %d/%d",
				     aip->ai_id, aip->ai_mode);
#endif
				return((aip->ai_mode & mode) ? 1 : 0);
			}
		}
		break;
	case ACL_IDTYPE_WORLD:
		break;
	default:
		elog(WARN, "aclcheck: bogus ACL id type");
		break;
	}
	
#ifdef ACLDEBUG_TRACE
	elog(DEBUG, "aclcheck: using world=%d", aidat->ai_mode);
#endif
	return((aidat->ai_mode & mode) ? 1 : 0);
}

int32
pg_aclcheck(relname, usename, mode)
	char *relname, *usename; /* string, not Name */
	AclMode mode;
{
	HeapTuple htp;
	AclId id;
	Acl *acl = (Acl *) NULL;
	int32 result;

	htp = SearchSysCacheTuple(USENAME, usename, NULL, NULL, NULL);
	if (!HeapTupleIsValid(htp))
		elog(WARN, "pg_aclcheck: usename=%s not found", usename);
	id = (AclId) ((Form_pg_user) GETSTRUCT(htp))->usesysid;

	/* Deny anyone permission to update a system catalog unless
	 * pg_user.usecatupd is set.  (This is to let superusers protect
	 * themselves from themselves.)
	 */
	if (((mode | ACL_WR) || (mode | ACL_AP)) &&
	    NameIsSystemRelationName(relname) &&
	    !((Form_pg_user) GETSTRUCT(htp))->usecatupd) {
		elog(NOTICE, "catalog update to \"%s\": permission denied",
		     relname);
		return(0);
	}
	
	/* Otherwise, superusers bypass all permission-checking.
	 */
	if (((Form_pg_user) GETSTRUCT(htp))->usesuper) {
#ifdef ACLDEBUG_TRACE
		elog(NOTICE, "pg_aclcheck: %s is superuser", usename);
#endif
		return(1);
	}
	
#ifndef ACLDEBUG
	htp = SearchSysCacheTuple(RELNAME, relname, NULL, NULL, NULL);
	if (!HeapTupleIsValid(htp)) {
		/* This could just be a historical relation, we may not care */
		elog(DEBUG, "pg_aclcheck: class \"%s\" not found", relname);
		return(1);
	}
	if (!heap_attisnull(htp, Anum_pg_relation_relacl))
		acl = &((Form_pg_relation) GETSTRUCT(htp))->relacl;
#else
	{ /* This is why the syscache is great... */
		Relation relation;
		static ScanKeyEntryData relkey[1] = {
			{ 0, Anum_pg_relation_relname, NameEqualRegProcedure }
		};
		HeapScanDesc hsdp;
		struct varlena *tmp;

		relation = heap_openr(Name_pg_relation);
		if (!RelationIsValid(relation)) {
			elog(NOTICE, "pg_checkacl: could not open \"%s\"??",
			     Name_pg_relation);
			return(1);
		}
		fmgr_info(NameEqualRegProcedure,
			  &relkey[0].func,
			  &relkey[0].nargs);
		relkey[0].argument = NameGetDatum(relname);
		hsdp = heap_beginscan(relation,
				      0,
				      NowTimeQual,
				      1,
				      (ScanKey) relkey);
		htp = heap_getnext(hsdp, 0, (Buffer *) 0);
		if (HeapTupleIsValid(htp) &&
		    !heap_attisnull(htp, Anum_pg_relation_relacl)) {
			tmp = (Acl *) heap_getattr(htp,
						   (Buffer) 0,
						   Anum_pg_relation_relacl,
						   &relation->rd_att,
						   (bool *) NULL);
			acl = (Acl *) palloc(VARSIZE(tmp));
			bcopy(tmp, acl, VARSIZE(tmp));
		}
		heap_endscan(hsdp);
		heap_close(relation);
	}
#endif
	result = aclcheck(acl, id, (AclIdType) ACL_IDTYPE_UID, mode);
	if (acl)
		pfree(acl);
	return(result);
}

int32
pg_ownercheck(relname, usename)
	char *relname, *usename; /* string, not Name */
{
	HeapTuple htp;
	AclId user_id, owner_id;

	htp = SearchSysCacheTuple(USENAME, usename, NULL, NULL, NULL);
	if (!HeapTupleIsValid(htp))
		elog(WARN, "pg_ownercheck: usename=%s not found", usename);
	user_id = (AclId) ((Form_pg_user) GETSTRUCT(htp))->usesysid;

	/* Superusers bypass all permission-checking.
	 */
	if (((Form_pg_user) GETSTRUCT(htp))->usesuper) {
#ifdef ACLDEBUG_TRACE
		elog(DEBUG, "pg_ownercheck: %s is superuser", usename);
#endif
		return(1);
	}

	htp = SearchSysCacheTuple(RELNAME, relname, NULL, NULL, NULL);
	if (!HeapTupleIsValid(htp))
		elog(WARN, "pg_ownercheck: relname=%s not found", relname);
	owner_id = ((Form_pg_relation) GETSTRUCT(htp))->relowner;
	return(user_id == owner_id);
}
