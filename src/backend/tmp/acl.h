/* ----------------------------------------------------------------
 *   FILE
 *     	acl.h
 *
 *   DESCRIPTION
 *     	Definition of (and support for) access control list data
 *	structures.
 *
 *   NOTES
 *	For backward-compatability purposes we have to allow there
 *	to be a null ACL in a pg_class tuple.  This will be defined as
 *	meaning "no protection" (i.e., old catalogs get old semantics).
 *
 *	The AclItems in an ACL array are currently kept in sorted order.
 *	Things will break hard if you change that without changing the
 *	code wherever this is included.
 *
 *   IDENTIFICATION
 *   	$Header$
 * ----------------------------------------------------------------
 */

#ifndef AclHIncluded			/* include only once */
#define AclHIncluded 1

#include "tmp/postgres.h"	/* for struct varlena */

/*
 * AclId	system identifier for the user, group, etc.
 *		XXX currently UNIX uid for users...
 */
typedef uint16 AclId;
#define	ACL_ID_WORLD	0	/* XXX only idtype should be checked */

/*
 * AclIdType	tag that describes if the AclId is a user, group, etc.
 */
typedef uint8 AclIdType;
#define	ACL_IDTYPE_WORLD	0x00
#define	ACL_IDTYPE_UID		0x01	/* user id - from pg_user */
#define	ACL_IDTYPE_GID		0x02	/* group id - from pg_group */

/*
 * AclMode	the actual permissions
 *		XXX should probably use bit.h routines.
 *		XXX should probably also stuff the modechg cruft in the
 *		    high bits, too.
 */
typedef uint8 AclMode;
#define	ACL_NO		0	/* no permissions */
#define	ACL_AP		(1<<0)	/* append */
#define	ACL_RD		(1<<1)	/* read */
#define	ACL_WR		(1<<2)	/* write (append/delete/replace) */
#define	ACL_RU		(1<<3)	/* place rules */
#define	N_ACL_MODES	4

#define	ACL_MODECHG_ADD		1
#define	ACL_MODECHG_DEL		2
#define	ACL_MODECHG_EQL		3

#define	ACL_WORLD_DEFAULT	(ACL_RD|ACL_WR|ACL_AP|ACL_RU)
#define	ACL_OWNER_DEFAULT	(ACL_RD|ACL_WR|ACL_AP|ACL_RU)

/*
 * AclItem
 */
typedef struct _AclItem {
	AclId		ai_id;
	AclIdType	ai_idtype;
	AclMode		ai_mode;
} AclItem;

/*
 * Acl		a POSTGRES array of AclItem
 *		XXX having structures follow immediately after the int4 vl_len
 *		    may cause a problem on machines with weird alignment
 *		    requirements.  this will have to be fixed in the struct
 *		    varlena definition (with padding) since it's a general
 *		    problem for the POSTGRES array construct.
 */
typedef struct varlena Acl;
#define	ACL_NUM(ACL) ((VARSIZE(ACL) - sizeof(VARSIZE(ACL))) / sizeof(AclItem))
#define	ACL_DAT(ACL) ((AclItem *) VARDATA(ACL))

/*
 * IdList	a POSTGRES array of AclId
 *		XXX having structures follow immediately after the int4 vl_len
 *		    may cause a problem on machines with weird alignment
 *		    requirements.  this will have to be fixed in the struct
 *		    varlena definition (with padding) since it's a general
 *		    problem for the POSTGRES array construct.
 */
typedef struct varlena IdList;
#define	IDLIST_NUM(IDL)	((VARSIZE(IDL) - sizeof(VARSIZE(IDL))) / sizeof(AclId))
#define	IDLIST_DAT(IDL)	((AclId *) VARDATA(IDL))

/*
 * Enable ACL execution tracing and table dumps
 */
/*#define ACLDEBUG_TRACE*/

/*
 * routines used internally (parser, etc.) 
 */
extern char *aclparse ARGS((char *s, AclItem *aip, unsigned *modechg));
extern Acl *acldefault ARGS((AclId owner));
extern Acl *aclinsert3 ARGS((Acl *acl, AclItem *aip, unsigned modechg));
/* XXX move these elsewhere */
extern int32 pg_aclcheck ARGS((char *relname, char *usename, AclMode mode));
extern int32 pg_ownercheck ARGS((char *relname, char *usename));

/*
 * exported routines (from acl.c)
 */
extern AclItem *aclitemin ARGS((char *s));
extern char *aclitemout ARGS((AclItem *acl));
extern Acl *aclinsert ARGS((Acl *acl, AclItem *aip));
extern Acl *aclremove ARGS((Acl *acl, AclItem *aip));
extern int32 aclcontains ARGS((Acl *acl, AclItem *aip));

#endif AclHIncluded
