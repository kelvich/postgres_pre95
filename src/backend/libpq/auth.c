/* ----------------------------------------------------------------
 *   FILE
 *	auth.c
 *	
 *   DESCRIPTION
 *	Routines to handle network authentication
 *
 *   INTERFACE ROUTINES
 *	pg_sendauth		send authentication information
 *	pg_recvauth		receive authentication information
 *
 *	fe_getauthname		get user's name according to the client side
 *				of the authentication system
 *	fe_setauthsvc		set frontend authentication service
 *	fe_getauthsvc		get current frontend authentication service
 *
 *	be_setauthsvc		do/do not permit an authentication service
 *	be_getauthsvc		is an authentication service permitted?
 *
 *   NOTES
 * 	These functions are used by both frontend applications and
 *	the postmaster.
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#include <stdio.h>
#include <sys/param.h>	/* for MAXHOSTNAMELEN */

#include "libpq/auth.h"
#include "tmp/pqcomm.h"

RcsId("$Header$");

extern char PQerrormsg[];

/* ----------------------------------------------------------------
 * MIT Kerberos authentication system - protocol version 4
 * ----------------------------------------------------------------
 */
#ifdef KRB4

#include "krb.h"

static 
char *
pg_krb4_authname()
{
	static char name[SNAME_SZ+1];
	char instance[INST_SZ];
	char realm[REALM_SZ];
	int status;
	
	bzero(name, sizeof(name));
	if ((status = krb_get_tf_fullname(tkt_string(), name, instance, realm))
	    != KSUCCESS) {
		sprintf(PQerrormsg, "pg_krb4_authname: krb_get_tf_fullname: %s\n",
			krb_err_txt[status]);
		fprintf(stderr, PQerrormsg);
		return((char *) NULL);
	}
	return(name);
}

static
pg_krb4_sendauth(sock, laddr, raddr, hostname)
	int sock;
	struct sockaddr_in *laddr, *raddr;
	char *hostname;
{
	long		krbopts = 0;	/* one-way authentication */
	KTEXT_ST	clttkt;
	int		status;
	char		hostbuf[MAXHOSTNAMELEN];

	if (!hostname) {
		if (gethostname(hostbuf, MAXHOSTNAMELEN) < 0)
			strcpy(hostbuf, "localhost");
		hostname = hostbuf;
	}
	status = krb_sendauth(krbopts,
			      sock,
			      &clttkt,
			      PG_KRB4_SERVICE,
			      hostname,
			      (char *) NULL,		/* current realm */
			      (u_long) 0,		/* !mutual */
			      (MSG_DAT *) NULL,		/* !mutual */
			      (CREDENTIALS *) NULL,	/* !mutual */
			      (Key_schedule *) NULL,	/* !mutual */
			      laddr,
			      raddr,
			      PG_KRB4_VERSION);
	if (status != KSUCCESS) {
		sprintf(PQerrormsg, "pg_krb4_sendauth: kerberos error: %s\n",
			krb_err_txt[status]);
		fprintf(stderr, PQerrormsg);
		return(STATUS_ERROR);
	}
	return(STATUS_OK);
}

static
pg_krb4_recvauth(sock, laddr, raddr, username)
	int sock;
	struct sockaddr_in *laddr, *raddr;
	char *username;
{
	long		krbopts = 0;	/* one-way authentication */
	KTEXT_ST	clttkt;
	char		instance[INST_SZ];
	AUTH_DAT	auth_data;
	Key_schedule	key_sched;
	char		version[KRB_SENDAUTH_VLEN];
	int		status;
	
	strcpy(instance, "*");	/* don't care, but arg gets expanded anyway */
	status = krb_recvauth(krbopts,
			      sock,
			      &clttkt,
			      PG_KRB4_SERVICE,
			      instance,
			      raddr,
			      laddr,
			      &auth_data,
			      PG_KRB4_SRVTAB,
			      key_sched,
			      version);
	if (status != KSUCCESS) {
		sprintf(PQerrormsg, "postmaster: kerberos error: %s\n",
			krb_err_txt[status]);
		fprintf(stderr, PQerrormsg);
		return(STATUS_ERROR);
	}
	if (strncmp(version, PG_KRB4_VERSION, KRB_SENDAUTH_VLEN)) {
		sprintf(PQerrormsg, "postmaster: protocol version != \"%s\"\n",
			PG_KRB4_VERSION);
		fprintf(stderr, PQerrormsg);
		return(STATUS_ERROR);
	}
	if (username && *username &&
	    strncmp(username, auth_data.pname, ANAME_SZ)) {
		sprintf(PQerrormsg, "postmaster: name \"%s\" != \"%s\"\n",
			username, auth_data.pname);
		fprintf(stderr, PQerrormsg);
		return(STATUS_ERROR);
	}
	return(STATUS_OK);
}

#endif /* KRB4 */

/* ----------------------------------------------------------------
 * MIT Kerberos authentication system - protocol version 5
 * ----------------------------------------------------------------
 */
#ifdef KRB5

static
char *
pg_krb5_authname()
{
}

static
pg_krb5_sendauth(sock, laddr, raddr, hostname)
	int sock;
	struct sockaddr_in *laddr, *raddr;
	char *hostname;
{
}

static
pg_krb5_recvauth(port, username)
	Port *port;
	char *username;
{
}

#endif /* KRB5 */

/* ----------------------------------------------------------------
 * common definitions
 * ----------------------------------------------------------------
 */

struct authsvc {
	char	name[16];	/* service nickname (for command line) */
	MsgType	msgtype;	/* startup packet header type */
	int	allowed;	/* initially allowed (before command line
				 * option parsing)?
				 */
};

/* Command-line parsing routines use this structure to map nicknames
 * onto service types (and the startup packets to use with them).
 *
 * Programs receiving an authentication request use this structure to
 * decide which authentication service types are currently permitted.
 * By default, all authentication systems compiled into the system are
 * allowed.  Unauthenticated connections are disallowed unless there
 * isn't any authentication system.
 */
static struct authsvc authsvcs[] = {
#ifdef KRB4
	{ "krb4",     STARTUP_KRB4_MSG, 1 },
#ifndef KRB5
	/* "kerberos" means KRB4 iff we don't have a later version.
	 * (there are lots of copies of KRB4 out there.)
	 */
	{ "kerberos", STARTUP_KRB4_MSG, 1 },
#endif /* !KRB5 */
#endif /* KRB4 */
#ifdef KRB5
	{ "krb5",     STARTUP_KRB5_MSG, 1 },
	{ "kerberos", STARTUP_KRB5_MSG, 1 },
#endif /* KRB5 */
	{ UNAUTHNAME, STARTUP_MSG,
#if defined(KRB4) || defined(KRB5)
					0
#else /* !(KRB4 || KRB5) */
					1
#endif /* !(KRB4 || KRB5) */
					  }
};

static n_authsvcs = sizeof(authsvcs) / sizeof(struct authsvc);

pg_sendauth(msgtype, port, hostname)
	MsgType msgtype;
	Port *port;
	char *hostname;
{
	switch (msgtype) {
#ifdef KRB4
	case STARTUP_KRB4_MSG:
		pg_krb4_sendauth(port->sock, &port->laddr, &port->raddr,
				 hostname);
		break;
#endif
#ifdef KRB5
	case STARTUP_KRB5_MSG:
		pg_krb5_sendauth(port->sock, &port->laddr, &port->raddr,
				 hostname);
		break;
#endif
	case STARTUP_MSG:
		break;
	}
}

pg_recvauth(msgtype, port, username)
	MsgType msgtype;
	Port *port;
	char *username;
{
	switch (msgtype) {
#ifdef KRB4
	case STARTUP_KRB4_MSG:
		if (!be_getauthsvc(msgtype)) {
			fprintf(stderr, "postmaster: krb4 authentication disallowed\n");
			return(STATUS_ERROR);
		}
		if (pg_krb4_recvauth(port->sock, &port->laddr, &port->raddr,
				     username) != STATUS_OK) {
			fprintf(stderr, "postmaster: krb4 authentication failed\n");
			return(STATUS_ERROR);
		}
		break;
#endif
#ifdef KRB5
	case STARTUP_KRB5_MSG:
		if (!be_getauthsvc(msgtype)) {
			fprintf(stderr, "postmaster: krb5 authentication disallowed\n");
			return(STATUS_ERROR);
		}
		if (pg_krb5_recvauth(port->sock, &port->laddr, &port->raddr,
				     username) != STATUS_OK) {
			fprintf(stderr, "postmaster: krb5 authentication failed\n");
			return(STATUS_ERROR);
		}
		break;
#endif
	case STARTUP_MSG:
		if (!be_getauthsvc(msgtype)) {
			fprintf(stderr, "postmaster: unauthenticated connections disallowed\n");
			return(STATUS_ERROR);
		}
		break;
	default:
		fprintf(stderr, "postmaster: unrecognized message type: %d\n",
			msgtype);
		return(STATUS_ERROR);
	}
	return(STATUS_OK);
}

/* ----------------------------------------------------------------
 * front-end routines
 * ----------------------------------------------------------------
 */
static pg_authsvc = -1;

void
fe_setauthsvc(name)
	char *name;
{
	int i;
	
	for (i = 0; i < n_authsvcs; ++i)
		if (!strcmp(name, authsvcs[i].name)) {
			pg_authsvc = i;
			break;
		}
	if (i == n_authsvcs)
		fprintf(stderr, "fe_setauthsvc: invalid name: %s, ignoring...\n",
			name);
	return;
}

MsgType
fe_getauthsvc()
{
	if (pg_authsvc < 0 || pg_authsvc >= n_authsvcs)
		fe_setauthsvc(DEFAULT_CLIENT_AUTHSVC);
	return(authsvcs[pg_authsvc].msgtype);
}

char *
fe_getauthname()
{
	char *name = (char *) NULL;
	MsgType authsvc;
	extern char *getenv();
	
	authsvc = fe_getauthsvc();
	switch ((int) authsvc) {
#ifdef KRB4
	case STARTUP_KRB4_MSG:
		name = pg_krb4_authname();
		break;
#endif
#ifdef KRB5
	case STARTUP_KRB5_MSG:
		name = pg_krb5_authname();
		break;
#endif
	case STARTUP_MSG:
		name = getenv("USER");
		break;
	default:
		fprintf(stderr, "fe_getauthname: invalid authentication system: %d\n",
			authsvc);
		break;
	}
	return(name);
}

/* ----------------------------------------------------------------
 * postmaster routines
 * ----------------------------------------------------------------
 */
void
be_setauthsvc(name)
	char *name;
{
	int i, j;
	int turnon = 1;

	if (!name)
		return;
	if (!strncmp("no", name, 2)) {
		turnon = 0;
		name += 2;
	}
	if (name[0] == '\0')
		return;
	for (i = 0; i < n_authsvcs; ++i)
		if (!strcmp(name, authsvcs[i].name)) {
			for (j = 0; j < n_authsvcs; ++j)
				if (authsvcs[j].msgtype == authsvcs[i].msgtype)
					authsvcs[j].allowed = turnon;
			break;
		}
	if (i == n_authsvcs)
		fprintf(stderr, "be_setauthsvc: invalid name %s, ignoring...\n",
			name);
	return;
}

int
be_getauthsvc(msgtype)
	MsgType msgtype;
{
	int i;

	for (i = 0; i < n_authsvcs; ++i)
		if (msgtype == authsvcs[i].msgtype)
			return(authsvcs[i].allowed);
	return(0);
}
