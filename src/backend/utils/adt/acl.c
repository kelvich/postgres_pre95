/* ----------------------------------------------------------------
 *   FILE
 *     	acl.c
 *
 *   DESCRIPTION
 *     	Basic access control list data structures manipulation routines.
 *
 *   NOTES
 *	See acl.h.
 *
 *   IDENTIFICATION
 *   	$Header$
 * ----------------------------------------------------------------
 */

#include <ctype.h>
#include "strings.h"
#include "tmp/acl.h"
#include "access/htup.h"
#include "catalog/pg_user.h"
#include "catalog/syscache.h"
#include "utils/log.h"
#include "utils/palloc.h"

static char *getid ARGS((char *s, char *n));
static int32 aclitemeq ARGS((AclItem *a1, AclItem *a2));
static int32 aclitemgt ARGS((AclItem *a1, AclItem *a2));

#define	ACL_IDTYPE_GID_KEYWORD	"group"
#define	ACL_IDTYPE_UID_KEYWORD	"user"

#define	ACL_MODECHG_STR		"+-="	/* list of valid characters */
#define	ACL_MODECHG_ADD_CHR	'+'
#define	ACL_MODECHG_DEL_CHR	'-'
#define	ACL_MODECHG_EQL_CHR	'='
#define	ACL_MODE_STR		"arwR"	/* list of valid characters */
#define	ACL_MODE_AP_CHR		'a'
#define	ACL_MODE_RD_CHR		'r'
#define	ACL_MODE_WR_CHR		'w'
#define	ACL_MODE_RU_CHR		'R'

/*
 * getid
 *	Consumes the first alphanumeric string (identifier) found in string
 *	's', ignoring any leading white space.
 *
 * RETURNS:
 *	the string position in 's' immediately following the identifier.  Also:
 *	- loads the identifier into 'name'.  (If no identifier is found, 'name'
 *	  contains an empty string).
 */
static
char *
getid(s, n)
	char *s;
	char *n;
{
	unsigned len;
	char *id;
	
	Assert(s && n);

	while (isspace(*s))
		++s;
	for (id = s, len = 0; isalnum(*s); ++len, ++s)
		;
	if (len > sizeof(NameData))
		elog(WARN, "getid: identifier cannot be >%d characters",
		     sizeof(NameData));
	if (len > 0)
		bcopy(id, n, len);
	n[len] = '\0';
	return(s);
}

/*
 * aclparse
 *	Consumes and parses an ACL specification of the form:
 *		[group|user] [A-Za-z0-9]*[+-=][rwaR]*
 *	from string 's', ignoring any leading white space or white space
 *	between the optional id type keyword (group|user) and the actual
 *	ACL specification.
 *
 *	This routine is called by the parser as well as aclitemin(), hence
 *	the added generality.
 *
 * RETURNS:
 *	the string position in 's' immediately following the ACL
 *	specification.  Also:
 *	- loads the structure pointed to by 'aip' with the appropriate
 *	  UID/GID, id type identifier and mode type values.
 *	- loads 'modechg' with the mode change flag.
 */
char *
aclparse(s, aip, modechg)
	char *s;
	AclItem *aip;
	unsigned *modechg;
{
	HeapTuple htp;
	char name[sizeof(NameData)+1];
	extern AclId get_grosysid();
	
	Assert(s && aip && modechg);

	aip->ai_idtype = ACL_IDTYPE_UID;
	s = getid(s, name);
	if (isspace(*s)) {	/* we just read a keyword, not a name */
		if (!strcmp(name, ACL_IDTYPE_GID_KEYWORD)) {
			aip->ai_idtype = ACL_IDTYPE_GID;
		} else if (strcmp(name, ACL_IDTYPE_UID_KEYWORD)) {
			elog(WARN, "aclparse: bad keyword");
		}
		s = getid(s, name);
		if (name[0] == '\0')
			elog(WARN, "aclparse: a name must follow a keyword");
	}
	if (name[0] == '\0')
		aip->ai_idtype = ACL_IDTYPE_WORLD;
	
	switch (*s) {
	case ACL_MODECHG_ADD_CHR: *modechg = ACL_MODECHG_ADD; break;
	case ACL_MODECHG_DEL_CHR: *modechg = ACL_MODECHG_DEL; break;
	case ACL_MODECHG_EQL_CHR: *modechg = ACL_MODECHG_EQL; break;
	default:  elog(WARN, "aclparse: mode change flag must use \"%s\"",
		       ACL_MODECHG_STR);
	}

	aip->ai_mode = ACL_NO;
	while (isalpha(*++s)) {
		switch (*s) {
		case ACL_MODE_AP_CHR: aip->ai_mode |= ACL_AP; break;
		case ACL_MODE_RD_CHR: aip->ai_mode |= ACL_RD; break;
		case ACL_MODE_WR_CHR: aip->ai_mode |= ACL_WR; break;
		case ACL_MODE_RU_CHR: aip->ai_mode |= ACL_RU; break;
		default:  elog(WARN, "aclparse: mode flags must use \"%s\"",
			       ACL_MODE_STR);
		}
	}

	switch (aip->ai_idtype) {
	case ACL_IDTYPE_UID:
		htp = SearchSysCacheTuple(USENAME, name, NULL, NULL, NULL);
		if (!HeapTupleIsValid(htp))
			elog(WARN, "aclparse: non-existent user \"%s\"", name);
		aip->ai_id = ((Form_pg_user) GETSTRUCT(htp))->usesysid;
		break;
	case ACL_IDTYPE_GID:
		aip->ai_id = get_grosysid(name);
		break;
	case ACL_IDTYPE_WORLD:
		aip->ai_id = ACL_ID_WORLD;
		break;
	}

#ifdef ACLDEBUG_TRACE
	elog(DEBUG, "aclparse: correctly read [%x %d %x], modechg=%x",
	     aip->ai_idtype, aip->ai_id, aip->ai_mode, *modechg);
#endif
	return(s);
}

/*
 * makeacl
 *	Allocates storage for a new Acl with 'n' entries.
 *
 * RETURNS:
 *	the new Acl
 */
Acl *
makeacl(n)
    int n;
{
    Acl *new_acl;
    Size size;

    if (n < 0)
	elog(WARN, "makeacl: invalid size: %d\n", n);
    size = ACL_N_SIZE(n);
    if (!(new_acl = (Acl *) palloc(size)))
	elog(WARN, "makeacl: palloc failed on %d\n", size);
    bzero((char *) new_acl, size);
    new_acl->size = size;
    new_acl->ndim = 1;
    new_acl->flags = 0;
    ARR_LBOUND(new_acl)[0] = 0;
    ARR_DIMS(new_acl)[0] = n;
    return(new_acl);
}

/*
 * aclitemin
 *	Allocates storage for, and fills in, a new AclItem given a string
 *	's' that contains an ACL specification.  See aclparse for details.
 *
 * RETURNS:
 *	the new AclItem
 */
AclItem *
aclitemin(s)
	char *s;
{
	unsigned modechg;
	AclItem *aip;

	if (!s)
		elog(WARN, "aclitemin: null string");

	aip = (AclItem *) palloc(sizeof(AclItem));
	if (!aip)
		elog(WARN, "aclitemin: palloc failed");
	s = aclparse(s, aip, &modechg);
	if (modechg != ACL_MODECHG_EQL)
		elog(WARN, "aclitemin: cannot accept anything but = ACLs");
	while (isspace(*s))
		++s;
	if (*s)
		elog(WARN, "aclitemin: extra garbage at end of specification");
	return(aip);
}

/*
 * aclitemout
 *	Allocates storage for, and fills in, a new null-delimited string
 *	containing a formatted ACL specification.  See aclparse for details.
 *
 * RETURNS:
 *	the new string
 */
char *
aclitemout(aip)
	AclItem *aip;
{
	register char *p;
	char *out;
	HeapTuple htp;
	unsigned i;
	static AclItem default_aclitem = { ACL_ID_WORLD,
					   ACL_IDTYPE_WORLD,
					   ACL_WORLD_DEFAULT };
	extern char *int2out();
	Name tmpname;
	extern Name get_groname();

	if (!aip)
		aip = &default_aclitem;
	
	p = out = palloc(sizeof("group =arwR ") + NAMEDATALEN);
	if (!out)
		elog(WARN, "aclitemout: palloc failed");
	*p = '\0';

	switch (aip->ai_idtype) {
	case ACL_IDTYPE_UID:
		htp = SearchSysCacheTuple(USESYSID, (char *) aip->ai_id,
					  NULL, NULL, NULL);
		if (!HeapTupleIsValid(htp)) {
			char *tmp = int2out(aip->ai_id);

			elog(NOTICE, "aclitemout: usesysid %d not found",
			     aip->ai_id);
			(void) strcat(p, tmp);
			pfree(tmp);
		} else
			(void) strncat(p, (char *) &((Form_pg_user)
						     GETSTRUCT(htp))->usename,
				       sizeof(NameData));
		break;
	case ACL_IDTYPE_GID:
		(void) strcat(p, "group ");
		tmpname = get_groname(aip->ai_id);
		(void) strncat(p, tmpname->data, sizeof(NameData));
		pfree((char *) tmpname);
		break;
	case ACL_IDTYPE_WORLD:
		break;
	default:
		elog(WARN, "aclitemout: bad ai_idtype: %d", aip->ai_idtype);
		break;
	}
	while (*p)
		++p;
	*p++ = '=';
	for (i = 0; i < N_ACL_MODES; ++i)
		if ((aip->ai_mode >> i) & 01)
			*p++ = ACL_MODE_STR[i];
	*p = '\0';

	return(out);
}

/*
 * aclitemeq
 * aclitemgt
 *	AclItem equality and greater-than comparison routines.
 *	Two AclItems are equal iff they are both NULL or they have the
 *	same identifier (and identifier type).
 *
 * RETURNS:
 *	a boolean value indicating = or >
 */
static
int32
aclitemeq(a1, a2)
	AclItem *a1, *a2;
{
	if (!a1 && !a2)
		return(1);
	if (!a1 || !a2)
		return(0);
	return(a1->ai_idtype == a2->ai_idtype && a1->ai_id == a2->ai_id);
}

static
int32
aclitemgt(a1, a2)
	AclItem *a1, *a2;
{
	if (a1 && !a2)
		return(1);
	if (!a1 || !a2)
		return(0);
	return((a1->ai_idtype > a2->ai_idtype) ||
	       (a1->ai_idtype == a2->ai_idtype && a1->ai_id > a2->ai_id));
}

Acl *
acldefault(ownerid)
	AclId ownerid;
{
	Acl *acl;
	AclItem *aip;
	unsigned size;
	
	acl = makeacl(1);
	aip = ACL_DAT(acl);
	aip[0].ai_idtype = ACL_IDTYPE_WORLD;
	aip[0].ai_id = ACL_ID_WORLD;
	aip[0].ai_mode = ACL_WORLD_DEFAULT;
	return(acl);
}

Acl *
aclinsert3(old_acl, mod_aip, modechg)
	Acl *old_acl;
	AclItem *mod_aip;
	unsigned modechg;
{
	Acl *new_acl;
	AclItem *old_aip, *new_aip;
	unsigned src, dst, num;
	
	if (!old_acl || ACL_NUM(old_acl) < 1) {
		new_acl = makeacl(0);
		return(new_acl);
	}
	if (!mod_aip) {
		new_acl = makeacl(ACL_NUM(old_acl));
		bcopy((char *) old_acl, (char *) new_acl, ACL_SIZE(old_acl));
		return(new_acl);
	}
		
	num = ACL_NUM(old_acl);
	old_aip = ACL_DAT(old_acl);
	
	/*
	 * Search the ACL for an existing entry for 'id'.  If one exists,
	 * just modify the entry in-place (well, in the same position, since
	 * we actually return a copy); otherwise, insert the new entry in
	 * sort-order.
	 */
	/* find the first element not less than the element to be inserted */
	for (dst = 0; dst < num && aclitemgt(mod_aip, old_aip+dst); ++dst)
		;
	if (dst < num && aclitemeq(mod_aip, old_aip+dst)) {
							/* modify in-place */
		new_acl = makeacl(ACL_NUM(old_acl));
		bcopy((char *) old_acl, (char *) new_acl, ACL_SIZE(old_acl));
		new_aip = ACL_DAT(new_acl);
		src = dst;
	} else {
		new_acl = makeacl(ACL_NUM(old_acl) + 1);
		new_aip = ACL_DAT(new_acl);
		if (dst == 0) {				/* start */
			elog(WARN, "aclinsert3: insertion before world ACL??");
		} else if (dst >= num) {		/* end */
			bcopy((char *) old_aip,
			      (char *) new_aip,
			      num * sizeof(AclItem));
		} else {				/* middle */
			bcopy((char *) old_aip,
			      (char *) new_aip,
			      dst * sizeof(AclItem));
			bcopy((char *) (old_aip+dst),
			      (char *) (new_aip+dst+1),
			      (num - dst) * sizeof(AclItem));
		}
		new_aip[dst].ai_id = mod_aip->ai_id;
		new_aip[dst].ai_idtype = mod_aip->ai_idtype;
		src = 0;	/* world entry */
	}
	switch (modechg) {
	case ACL_MODECHG_ADD: new_aip[dst].ai_mode =
		old_aip[src].ai_mode | mod_aip->ai_mode;
		break;
	case ACL_MODECHG_DEL: new_aip[dst].ai_mode =
		old_aip[src].ai_mode & ~mod_aip->ai_mode;
		break;
	case ACL_MODECHG_EQL: new_aip[dst].ai_mode =
		mod_aip->ai_mode;
		break;
	}
	return(new_acl);
}

/*
 * aclinsert
 *
 */
Acl *
aclinsert(old_acl, mod_aip)
	Acl *old_acl;
	AclItem *mod_aip;
{
	return(aclinsert3(old_acl, mod_aip, ACL_MODECHG_EQL));
}

Acl *
aclremove(old_acl, mod_aip)
	Acl *old_acl;
	AclItem *mod_aip;
{
	Acl *new_acl;
	AclItem *old_aip, *new_aip;
	unsigned dst, old_num, new_num;
	
	if (!old_acl || ACL_NUM(old_acl) < 1) {
		new_acl = makeacl(0);
		return(new_acl);
	}
	if (!mod_aip) {
		new_acl = makeacl(ACL_NUM(old_acl));
		bcopy((char *) old_acl, (char *) new_acl, ACL_SIZE(old_acl));
		return(new_acl);
	}
	
	old_num = ACL_NUM(old_acl);
	old_aip = ACL_DAT(old_acl);
	
	for (dst = 0; dst < old_num && !aclitemeq(mod_aip, old_aip+dst); ++dst)
		;
	if (dst >= old_num) {			/* not found or empty */
		new_acl = makeacl(ACL_NUM(old_acl));
		bcopy((char *) old_acl, (char *) new_acl, ACL_SIZE(old_acl));
	} else {
		new_num = old_num - 1;
		new_acl = makeacl(ACL_NUM(old_acl) - 1);
		new_aip = ACL_DAT(new_acl);
		if (dst == 0) {			/* start */
			elog(WARN, "aclremove: removal of the world ACL??");
		} else if (dst == old_num - 1) {/* end */
			bcopy((char *) old_aip,
			      (char *) new_aip,
			      new_num * sizeof(AclItem));
		} else {			/* middle */
			bcopy((char *) old_aip,
			      (char *) new_aip,
			      dst * sizeof(AclItem));
			bcopy((char *) (old_aip+dst+1),
			      (char *) (new_aip+dst),
			      (new_num - dst) * sizeof(AclItem));
		}
	}
	return(new_acl);
}

int32
aclcontains(acl, aip)
	Acl *acl;
	AclItem *aip;
{
	unsigned i, num;
	AclItem *aidat;

	if (!acl || !aip || ((num = ACL_NUM(acl)) < 1))
		return(0);
	aidat = ACL_DAT(acl);
	for (i = 0; i < num; ++i)
		if (aclitemeq(aip, aidat+i))
			return(1);
	return(0);
}
