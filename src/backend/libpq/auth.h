/*----------------------------------------------------------------
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
 *----------------------------------------------------------------
 */

#ifndef AuthIncluded		/* Include this file only once */
#define	AuthIncluded	1

#include "tmp/c.h"
#include "tmp/pqcomm.h"

/*----------------------------------------------------------------
 * Common routines and definitions
 *----------------------------------------------------------------
 */

/* what we call "no authentication system" */
#define	UNAUTHNAME		"unauth"

/* what a frontend uses by default */
#if !defined(KRB4) && !defined(KRB5)
#define	DEFAULT_CLIENT_AUTHSVC	UNAUTHNAME
#else /* KRB4 || KRB5 */
#define	DEFAULT_CLIENT_AUTHSVC	"kerberos"
#endif /* KRB4 || KRB5 */

extern		fe_sendauth ARGS((MsgType msgtype,
				  Port *port,
				  char *hostname));
extern char    *fe_getauthname ARGS(());
extern void	fe_setauthsvc ARGS((char *name));
extern MsgType	fe_getauthsvc ARGS(());

extern		be_recvauth ARGS((Msgtype msgtype,
				  Port *port,
				  char *username));
extern void	be_setauthsvc ARGS((char *name));
extern int	be_getauthsvc ARGS(());

#define	PG_KRB4_VERSION	"PGVER4.1"	/* at most KRB_SENDAUTH_VLEN chars */
#define	PG_KRB5_VERSION	"PGVER5.1"

#endif /* !defined(AuthIncluded) */
