/*----------------------------------------------------------------
 *   FILE
 *	auth.c
 *	
 *   DESCRIPTION
 *	Routines to handle network authentication
 *
 *   INTERFACE ROUTINES
 *     frontend (client) routines:
 *	fe_sendauth		send authentication information
 *	fe_getauthname		get user's name according to the client side
 *				of the authentication system
 *	fe_setauthsvc		set frontend authentication service
 *	fe_getauthsvc		get current frontend authentication service
 *
 *     backend (postmaster) routines:
 *	be_recvauth		receive authentication information
 *	be_setauthsvc		do/do not permit an authentication service
 *	be_getauthsvc		is an authentication service permitted?
 *
 *   NOTES
 *	To add a new authentication system:
 *	0. If you can't do your authentication over an existing socket,
 *	   you lose -- get ready to hack around this framework instead of 
 *	   using it.  Otherwise, you can assume you have an initialized
 *	   and empty connection to work with.  (Please don't leave leftover
 *	   gunk in the connection after the authentication transactions, or
 *	   the POSTGRES routines that follow will be very unhappy.)
 *	1. Write a set of routines that:
 *		let a client figure out what user/principal name to use
 *		send authentication information (client side)
 *		receive authentication information (server side)
 *	   You can include both routines in this file, using #ifdef FRONTEND
 *	   to separate them.
 *	2. Edit libpq/pqcomm.h and assign a MsgType for your protocol.
 *	3. Edit the static "struct authsvc" array and the generic 
 *	   {be,fe}_{get,set}auth{name,svc} routines in this file to reflect 
 *	   the new service.  You may have to change the arguments of these
 *	   routines; they basically just reflect what Kerberos v4 needs.
 *	4. Hack on src/{,bin}/Makefile.global and src/{backend,libpq}/Makefile
 *	   to add library and CFLAGS hooks -- basically, grep the Makefile
 *	   hierarchy for KRBVERS to see where you need to add things.
 *
 *	Send mail to post_hackers@postgres.Berkeley.EDU if you have to make 
 *	any changes to arguments, etc.  Context diffs would be nice, too.
 *
 *	Someday, this cruft will go away and magically be replaced by a
 *	nice interface based on the GSS API or something.  For now, though,
 *	there's no (stable) UNIX security API to work with...
 *
 *   IDENTIFICATION
 *	$Header$
 *----------------------------------------------------------------
 */

#include <stdio.h>
#include <strings.h>
#include <sys/param.h>	/* for MAX{HOSTNAME,PATH}LEN, NOFILE */

#include "libpq/auth.h"
#include "tmp/pqcomm.h"

extern char	*getenv ARGS((const char *name));	/* XXX STDLIB */

RcsId("$Header$");

/*----------------------------------------------------------------
 * common definitions for generic fe/be routines
 *----------------------------------------------------------------
 */

struct authsvc {
	char	name[16];	/* service nickname (for command line) */
	MsgType	msgtype;	/* startup packet header type */
	int	allowed;	/* initially allowed (before command line
				 * option parsing)?
				 */
};

/*
 * Command-line parsing routines use this structure to map nicknames
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
	{ "kerberos", STARTUP_KRB4_MSG, 1 },
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

#ifdef KRB4
/*----------------------------------------------------------------
 * MIT Kerberos authentication system - protocol version 4
 *----------------------------------------------------------------
 */

#include "krb.h"

#ifdef FRONTEND

extern char	*tkt_string ARGS((void));

/*
 * pg_krb4_init -- initialization performed before any Kerberos calls are made
 *
 * For v4, all we need to do is make sure the library routines get the right
 * ticket file if we want them to see a special one.  (They will open the file
 * themselves.)
 */
static
void
pg_krb4_init()
{
	char		*realm;
	static		init_done = 0;
	
	if (init_done)
		return;
	init_done = 1;

	/*
	 * If the user set PGREALM, then we use a ticket file with a special
	 * name: <usual-ticket-file-name>@<PGREALM-value>
	 */
	if (realm = getenv("PGREALM")) {
		char	tktbuf[MAXPATHLEN];
		
		(void) sprintf(tktbuf, "%s@%s", tkt_string(), realm);
		krb_set_tkt_string(tktbuf);
	}
}

/*
 * pg_krb4_authname -- returns a pointer to static space containing whatever
 *		       name the user has authenticated to the system
 *
 * We obtain this information by digging around in the ticket file.
 */
static 
char *
pg_krb4_authname()
{
	char instance[INST_SZ];
	char realm[REALM_SZ];
	int status;
	static char name[SNAME_SZ+1] = "";
	
	if (name[0])
		return(name);

	pg_krb4_init();

	name[SNAME_SZ] = '\0';
	status = krb_get_tf_fullname(tkt_string(), name, instance, realm);
	if (status != KSUCCESS) {
		fprintf(stderr, "pg_krb4_authname: krb_get_tf_fullname: %s\n",
			krb_err_txt[status]);
		return((char *) NULL);
	}
	return(name);
}

/*
 * pg_krb4_sendauth -- client routine to send authentication information to
 *		       the server
 *
 * This routine does not do mutual authentication, nor does it return enough
 * information to do encrypted connections.  But then, if we want to do
 * encrypted connections, we'll have to redesign the whole RPC mechanism
 * anyway.
 *
 * If the user is too lazy to feed us a hostname, we try to come up with
 * something other than "localhost" since the hostname is used as an
 * instance and instance names in v4 databases are usually actual hostnames
 * (canonicalized to omit all domain suffixes).
 */
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
	char		*realm = getenv("PGREALM"); /* NULL == current realm */

	if (!hostname || !(*hostname)) {
		if (gethostname(hostbuf, MAXHOSTNAMELEN) < 0)
			strcpy(hostbuf, "localhost");
		hostname = hostbuf;
	}

	pg_krb4_init();

	status = krb_sendauth(krbopts,
			      sock,
			      &clttkt,
			      PG_KRB_SRVNAM,
			      hostname,
			      realm,
			      (u_long) 0,
			      (MSG_DAT *) NULL,
			      (CREDENTIALS *) NULL,
			      (Key_schedule *) NULL,
			      laddr,
			      raddr,
			      PG_KRB4_VERSION);
	if (status != KSUCCESS) {
		fprintf(stderr, "pg_krb4_sendauth: kerberos error: %s\n",
			krb_err_txt[status]);
		return(STATUS_ERROR);
	}
	return(STATUS_OK);
}

#else /* !FRONTEND */

/*
 * pg_krb4_recvauth -- server routine to receive authentication information
 *		       from the client
 *
 * Nothing unusual here, except that we compare the username obtained from
 * the client's setup packet to the authenticated name.  (We have to retain
 * the name in the setup packet since we have to retain the ability to handle
 * unauthenticated connections.)
 */
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
			      PG_KRB_SRVNAM,
			      instance,
			      raddr,
			      laddr,
			      &auth_data,
			      PG_KRB_SRVTAB,
			      key_sched,
			      version);
	if (status != KSUCCESS) {
		fprintf(stderr, "pg_krb4_recvauth: kerberos error: %s\n",
			krb_err_txt[status]);
		return(STATUS_ERROR);
	}
	if (strncmp(version, PG_KRB4_VERSION, KRB_SENDAUTH_VLEN)) {
		fprintf(stderr, "pg_krb4_recvauth: protocol version != \"%s\"\n",
			PG_KRB4_VERSION);
		return(STATUS_ERROR);
	}
	if (username && *username &&
	    /* XXX use smaller of ANAME_SZ and sizeof(NameData) */
	    strncmp(username, auth_data.pname, sizeof(NameData))) {
		fprintf(stderr, "pg_krb4_recvauth: name \"%-*s\" != \"%-*s\"\n",
			sizeof(NameData), username,
			sizeof(NameData), auth_data.pname);
		return(STATUS_ERROR);
	}
	return(STATUS_OK);
}

#endif /* !FRONTEND */

#endif /* KRB4 */

#ifdef KRB5
/*----------------------------------------------------------------
 * MIT Kerberos authentication system - protocol version 5
 *----------------------------------------------------------------
 */

#include "krb5/krb5.h"

/*
 * pg_an_to_ln -- return the local name corresponding to an authentication
 *		  name
 *
 * XXX Assumes that the first aname component is the user name.  This is NOT
 *     necessarily so, since an aname can actually be something out of your
 *     worst X.400 nightmare, like
 *	  ORGANIZATION=U. C. Berkeley/NAME=Paul M. Aoki@CS.BERKELEY.EDU
 *     Note that the MIT an_to_ln code does the same thing if you don't
 *     provide an aname mapping database...it may be a better idea to use
 *     krb5_an_to_ln, except that it punts if multiple components are found,
 *     and we can't afford to punt.
 */
static
char *
pg_an_to_ln(aname)
	char *aname;
{
	char	*p;

	if ((p = strchr(aname, '/')) || (p = strchr(aname, '@')))
		*p = '\0';
	return(aname);
}

#ifdef FRONTEND

/*
 * pg_krb5_init -- initialization performed before any Kerberos calls are made
 *
 * With v5, we can no longer set the ticket (credential cache) file name;
 * we now have to provide a file handle for the open (well, "resolved")
 * ticket file everywhere.
 * 
 */
static
krb5_ccache
pg_krb5_init()
{
	krb5_error_code		code;
	char			*realm, *defname;
	char			tktbuf[MAXPATHLEN];
	static krb5_ccache	ccache = (krb5_ccache) NULL;

	if (ccache)
		return(ccache);

	/*
	 * If the user set PGREALM, then we use a ticket file with a special
	 * name: <usual-ticket-file-name>@<PGREALM-value>
	 */
	if (!(defname = krb5_cc_default_name())) {
		fprintf(stderr, "pg_krb5_init: krb5_cc_default_name failed\n");
		return((krb5_ccache) NULL);
	}
	(void) strcpy(tktbuf, defname);
	if (realm = getenv("PGREALM")) {
		(void) strcat(tktbuf, "@");
		(void) strcat(tktbuf, realm);
	}
	
	if (code = krb5_cc_resolve(tktbuf, &ccache)) {
		com_err("pg_krb5_init", code, "in krb5_cc_resolve");
		return((krb5_ccache) NULL);
	}
	return(ccache);
}

/*
 * pg_krb5_authname -- returns a pointer to static space containing whatever
 *		       name the user has authenticated to the system
 *
 * We obtain this information by digging around in the ticket file.
 */
static
char *
pg_krb5_authname()
{
	krb5_ccache	ccache;
	krb5_principal	principal;
	krb5_error_code	code;
	static char	*authname = (char *) NULL;

	if (authname)
		return(authname);

	ccache = pg_krb5_init();	/* don't free this */
	
	if (code = krb5_cc_get_principal(ccache, &principal)) {
		com_err("pg_krb5_authname", code, "in krb5_cc_get_principal");
		return((char *) NULL);
	}
	if (code = krb5_unparse_name(principal, &authname)) {
		com_err("pg_krb5_authname", code, "in krb5_unparse_name");
		krb5_free_principal(principal);
		return((char *) NULL);
	}
	krb5_free_principal(principal);
	return(pg_an_to_ln(authname));
}

/*
 * pg_krb5_sendauth -- client routine to send authentication information to
 *		       the server
 *
 * This routine does not do mutual authentication, nor does it return enough
 * information to do encrypted connections.  But then, if we want to do
 * encrypted connections, we'll have to redesign the whole RPC mechanism
 * anyway.
 *
 * Server hostnames are canonicalized v4-style, i.e., all domain suffixes
 * are simply chopped off.  Hence, we are assuming that you've entered your
 * server instances as
 *	<value-of-PG_KRB_SRVNAM>/<canonicalized-hostname>
 * in the PGREALM (or local) database.  This is probably a bad assumption.
 */
static
pg_krb5_sendauth(sock, laddr, raddr, hostname)
	int sock;
	struct sockaddr_in *laddr, *raddr;
	char *hostname;
{
	char			servbuf[MAXHOSTNAMELEN + 1 +
					sizeof(PG_KRB_SRVNAM)];
	char			*hostp;
	char			*realm;
	krb5_error_code		code;
	krb5_principal		client, server;
	krb5_ccache		ccache;
	krb5_error		*error = (krb5_error *) NULL;

	ccache = pg_krb5_init();	/* don't free this */
	
	/*
	 * set up client -- this is easy, we can get it out of the ticket
	 * file.
	 */
	if (code = krb5_cc_get_principal(ccache, &client)) {
		com_err("pg_krb5_sendauth", code, "in krb5_cc_get_principal");
		return(STATUS_ERROR);
	}
	
	/*
	 * set up server -- canonicalize as described above
	 */
	(void) strcpy(servbuf, PG_KRB_SRVNAM);
	*(hostp = servbuf + (sizeof(PG_KRB_SRVNAM) - 1)) = '/';
	if (hostname || *hostname) {
		(void) strncpy(++hostp, hostname, MAXHOSTNAMELEN);
	} else {
		if (gethostname(++hostp, MAXHOSTNAMELEN) < 0)
			(void) strcpy(hostp, "localhost");
	}
	if (hostp = strchr(hostp, '.'))
		*hostp = '\0';
	if (realm = getenv("PGREALM")) {
		(void) strcat(servbuf, "@");
		(void) strcat(servbuf, realm);
	}
	if (code = krb5_parse_name(servbuf, &server)) {
		com_err("pg_krb5_sendauth", code, "in krb5_parse_name");
		krb5_free_principal(client);
		return(STATUS_ERROR);
	}
	
	/*
	 * The only thing we want back from krb5_sendauth is an error status
	 * and any error messages.
	 */
	if (code = krb5_sendauth((krb5_pointer) &sock,
				 PG_KRB5_VERSION,
				 client,
				 server,
				 (krb5_flags) 0,
				 (krb5_checksum *) NULL,
				 (krb5_creds *) NULL,
				 ccache,
				 (krb5_int32 *) NULL,
				 (krb5_keyblock **) NULL,
				 &error,
				 (krb5_ap_rep_enc_part **) NULL)) {
		if ((code == KRB5_SENDAUTH_REJECTED) && error)
			fprintf(stderr, "pg_krb5_sendauth: authentication rejected: \"%*s\"\n",
				error->text.length, error->text.data);
		else
			com_err("pg_krb5_sendauth", code, "in krb5_sendauth");
	}
	krb5_free_principal(client);
	krb5_free_principal(server);
	return(code ? STATUS_ERROR : STATUS_OK);
}

#else /* !FRONTEND */

/*
 * pg_krb4_recvauth -- server routine to receive authentication information
 *		       from the client
 *
 * We still need to compare the username obtained from the client's setup
 * packet to the authenticated name, as described in pg_krb4_recvauth.  This
 * is a bit more problematic in v5, as described above in pg_an_to_ln.
 *
 * In addition, as described above in pg_krb5_sendauth, we still need to
 * canonicalize the server name v4-style before constructing a principal
 * from it.  Again, this is kind of iffy.
 *
 * Finally, we need to tangle with the fact that v5 doesn't let you explicitly
 * set server keytab file names -- you have to feed lower-level routines a
 * function to retrieve the contents of a keytab, along with a single argument
 * that allows them to open the keytab.  We assume that a server keytab is
 * always a real file so we can allow people to specify their own filenames.
 * (This is important because the POSTGRES keytab needs to be readable by
 * non-root users/groups; the v4 tools used to force you do dump a whole
 * host's worth of keys into a file, effectively forcing you to use one file,
 * but kdb5_edit allows you to select which principals to dump.  Yay!)
 */
static
pg_krb5_recvauth(sock, laddr, raddr, username)
	int sock;
	struct sockaddr_in *laddr, *raddr;
	char *username;
{
	char			servbuf[MAXHOSTNAMELEN + 1 +
					sizeof(PG_KRB_SRVNAM)];
	char			*hostp, *kusername = (char *) NULL;
	krb5_error_code		code;
	krb5_principal		client, server;
	krb5_address		sender_addr;
	krb5_rdreq_key_proc	keyproc = (krb5_rdreq_key_proc) NULL;
	krb5_pointer		keyprocarg = (krb5_pointer) NULL;

	/*
	 * Set up server side -- since we have no ticket file to make this
	 * easy, we construct our own name and parse it.  See note on
	 * canonicalization above.
	 */
	(void) strcpy(servbuf, PG_KRB_SRVNAM);
	*(hostp = servbuf + (sizeof(PG_KRB_SRVNAM) - 1)) = '/';
	if (gethostname(++hostp, MAXHOSTNAMELEN) < 0)
		(void) strcpy(hostp, "localhost");
	if (hostp = strchr(hostp, '.'))
		*hostp = '\0';
	if (code = krb5_parse_name(servbuf, &server)) {
		com_err("pg_krb5_recvauth", code, "in krb5_parse_name");
		return(STATUS_ERROR);
	}
	
	/*
	 * krb5_sendauth needs this to verify the address in the client
	 * authenticator.
	 */
	sender_addr.addrtype = raddr->sin_family;
	sender_addr.length = sizeof(raddr->sin_addr);
	sender_addr.contents = (krb5_octet *) &(raddr->sin_addr);

	if (strcmp(PG_KRB_SRVTAB, "")) {
		keyproc = krb5_kt_read_service_key;
		keyprocarg = PG_KRB_SRVTAB;
	}

	if (code = krb5_recvauth((krb5_pointer) &sock,
				 PG_KRB5_VERSION,
				 server,
				 &sender_addr,
				 (krb5_pointer) NULL,
				 keyproc,
				 keyprocarg,
				 (char *) NULL,
				 (krb5_int32 *) NULL,
				 &client,
				 (krb5_ticket **) NULL,
				 (krb5_authenticator **) NULL)) {
		com_err("pg_krb5_recvauth", code, "in krb5_recvauth");
		krb5_free_principal(server);
		return(STATUS_ERROR);
	}
	krb5_free_principal(server);

	/*
	 * The "client" structure comes out of the ticket and is therefore
	 * authenticated.  Use it to check the username obtained from the
	 * postmaster startup packet.
	 */
	if ((code = krb5_unparse_name(client, &kusername))) {
		com_err("pg_krb5_recvauth", code, "in krb5_unparse_name");
		krb5_free_principal(client);
		return(STATUS_ERROR);
	}
	krb5_free_principal(client);
	if (!kusername) {
		fprintf(stderr, "pg_krb5_recvauth: could not decode username\n");
		return(STATUS_ERROR);
	}
	kusername = pg_an_to_ln(kusername);
	if (username && strncmp(username, kusername, sizeof(NameData))) {
		fprintf(stderr, "pg_krb5_recvauth: name \"%-*s\" != \"%-*s\"\n",
			sizeof(NameData), username,
			sizeof(NameData), kusername);
		free(kusername);
		return(STATUS_ERROR);
	}
	free(kusername);
	return(STATUS_OK);
}

#endif /* !FRONTEND */

#endif /* KRB5 */

#ifdef FRONTEND

/*----------------------------------------------------------------
 * front-end routines
 *----------------------------------------------------------------
 */

/*
 * fe_sendauth -- client demux routine for outgoing authentication information
 */
fe_sendauth(msgtype, port, hostname)
	MsgType msgtype;
	Port *port;
	char *hostname;
{
	switch (msgtype) {
#ifdef KRB4
	case STARTUP_KRB4_MSG:
		if (pg_krb4_sendauth(port->sock, &port->laddr, &port->raddr,
				     hostname) != STATUS_OK) {
			fprintf(stderr, "fe_sendauth: krb4 authentication failed\n");
			return(STATUS_ERROR);
		}
		break;
#endif
#ifdef KRB5
	case STARTUP_KRB5_MSG:
		if (pg_krb5_sendauth(port->sock, &port->laddr, &port->raddr,
				     hostname) != STATUS_OK) {
			fprintf(stderr, "fe_sendauth: krb5 authentication failed\n");
			return(STATUS_ERROR);
		}
		break;
#endif
	case STARTUP_MSG:
		break;
	}
	return(STATUS_OK);
}

/*
 * fe_setauthsvc
 * fe_getauthsvc
 *
 * Set/return the authentication service currently selected for use by the
 * frontend. (You can only use one in the frontend, obviously.)
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

/*
 * fe_getauthname -- returns a pointer to static space containing whatever
 *		     name the user has authenticated to the system
 */
char *
fe_getauthname()
{
	char *name = (char *) NULL;
	MsgType authsvc;
	
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
		name = getenv("USER");	/* getpwnam(getuid()) maybe??? */
		break;
	default:
		fprintf(stderr, "fe_getauthname: invalid authentication system: %d\n",
			authsvc);
		break;
	}
	return(name);
}

#else /* !FRONTEND */

/*----------------------------------------------------------------
 * postmaster routines
 *----------------------------------------------------------------
 */

/*
 * be_recvauth -- server demux routine for incoming authentication information
 */
be_recvauth(msgtype, port, username)
	MsgType msgtype;
	Port *port;
	char *username;
{
	if (!username) {
		fprintf(stderr, "be_recvauth: no user name passed\n");
		return(STATUS_ERROR);
	}
	if (!port) {
		fprintf(stderr, "be_recvauth: no port structure passed\n");
		return(STATUS_ERROR);
	}
			
	switch (msgtype) {
#ifdef KRB4
	case STARTUP_KRB4_MSG:
		if (!be_getauthsvc(msgtype)) {
			fprintf(stderr, "be_recvauth: krb4 authentication disallowed\n");
			return(STATUS_ERROR);
		}
		if (pg_krb4_recvauth(port->sock, &port->laddr, &port->raddr,
				     username) != STATUS_OK) {
			fprintf(stderr, "be_recvauth: krb4 authentication failed\n");
			return(STATUS_ERROR);
		}
		break;
#endif
#ifdef KRB5
	case STARTUP_KRB5_MSG:
		if (!be_getauthsvc(msgtype)) {
			fprintf(stderr, "be_recvauth: krb5 authentication disallowed\n");
			return(STATUS_ERROR);
		}
		if (pg_krb5_recvauth(port->sock, &port->laddr, &port->raddr,
				     username) != STATUS_OK) {
			fprintf(stderr, "be_recvauth: krb5 authentication failed\n");
			return(STATUS_ERROR);
		}
		break;
#endif
	case STARTUP_MSG:
		if (!be_getauthsvc(msgtype)) {
			fprintf(stderr, "be_recvauth: unauthenticated connections disallowed\n");
			return(STATUS_ERROR);
		}
		break;
	default:
		fprintf(stderr, "be_recvauth: unrecognized message type: %d\n",
			msgtype);
		return(STATUS_ERROR);
	}
	return(STATUS_OK);
}

/*
 * be_setauthsvc -- enable/disable the authentication services currently
 *		    selected for use by the backend
 * be_getauthsvc -- returns whether a particular authentication system
 *		    (indicated by its message type) is permitted by the
 *		    current selections
 *
 * be_setauthsvc encodes the command-line syntax that
 *	-a "<service-name>"
 * enables a service, whereas
 *	-a "no<service-name>"
 * disables it.
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
#endif /* !FRONTEND */
