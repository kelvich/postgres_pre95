/* ----------------------------------------------------------------
 *   FILE
 *	auth.h
 *	
 *   DESCRIPTION
 *	Definitions for network authentication routines
 *
 *   NOTES
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#ifndef AuthIncluded		/* Include this file only once */
#define	AuthIncluded	1

#include "tmp/c.h"
#include "tmp/pqcomm.h"

/* ----------------------------------------------------------------
 * Common routines and definitions
 * ----------------------------------------------------------------
 */

/* what we call "no authentication system" */
#define	UNAUTHNAME		"unauth"

/* what a frontend uses by default */
#define	DEFAULT_CLIENT_AUTHSVC	UNAUTHNAME

extern char    *fe_getauthname ARGS(());
extern void	fe_setauthsvc ARGS((char *name));
extern MsgType	fe_getauthsvc ARGS(());

extern void	be_setauthsvc ARGS((char *name));
extern int	be_getauthsvc ARGS(());

extern		pg_sendauth ARGS((MsgType msgtype, Port *port,
				  char *hostname));
extern		pg_recvauth ARGS((Msgtype msgtype, Port *port,
				  char *username));

/* ----------------------------------------------------------------
 * MIT Kerberos authentication system, protocol version 4
 *	Note that the srvtab you use must be readable by a non-root
 *	user, i.e., "postgres".
 * ----------------------------------------------------------------
 */
#ifdef KRB4
#define	PG_KRB4_SERVICE	"postgres_dbms"
#define	PG_KRB4_VERSION	"PGVER4.1"	/* at most KRB_SENDAUTH_VLEN chars */
#define	PG_KRB4_SRVTAB	"/etc/srvtab"
#endif /* KRB4 */

/* ----------------------------------------------------------------
 * MIT Kerberos authentication system, protocol version 5
 *	Note that the srvtab you use must be readable by a non-root
 *	user, i.e., "postgres".
 * ----------------------------------------------------------------
 */
#ifdef KRB5
#define	PG_KRB5_SERVICE	"postgres_dbms"
#define	PG_KRB5_VERSION	"PGVER5.1"	/* at most KRB_SENDAUTH_VLEN chars */
#define	PG_KRB5_SRVTAB	"/etc/srvtab"
#endif /* KRB5 */

#endif /* !defined(AuthIncluded) */
