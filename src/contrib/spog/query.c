/*
 * postgres part of spog
 *
 * $Id$
 *
 * $Log: query.c,v
 * Revision 2.2  1992/08/13  11:44:48  schoenw
 * options -x and -n added, postgres v4r0 support
 *
 * Revision 2.1  1992/05/22  12:53:33  schoenw
 * this is the public release 1.0
 *
 * Revision 1.1  1992/05/22  12:46:00  schoenw
 * Initial revision
 *
 */

#include "spog.h"

struct FMT_NODE *att_fmt = NULL;  /* formats used to print an attribute */
struct FMT_NODE *type_fmt = NULL; /* format used to print a specific type */
char *default_fmt = " %-12s";     /* separator printed at the end of a tupel */
char *separator = "\n";           /* separator printed at the end of a tupel */


char querybuffer[MAXQUERYSIZE] = "";
int  query_continues = 0;

/*
 *
 */

print_value (name, value, type)
char *name, *value;
int type;
{
	char *fmt = NULL;
	struct FMT_NODE *p = att_fmt;

	while (p) {
		if (!strcmp(p -> name, name)) {
			fmt = p -> fmt;
			break;
		}
		p = p -> next;
	}

	if (fmt == NULL) {
		p = type_fmt;
		while (p) {
			if (p -> oid == type ) {
				fmt = p -> fmt;
				break;
			}
			p = p -> next;
		}
	}

	if (fmt == NULL) {
		printf(default_fmt, value);
	} else {
		printf(fmt, value);
	}
}

/*
 * This is the routine that prints out the tuples that
 * are returned from the backend.
 */

print_tuples (pname)
char *pname;
{
	PortalBuffer *p;
	int i,j,k,g,m,n;
	int t=0;
	int total=0;
	
	/* Now to examine all tuples fetched. */
	
	p = PQparray(pname);
	g = PQngroups (p);

	if (verbose) {
		for (i=0; i<g; i++) total += PQntuplesGroup(p,i);
		printf ("Your query returned %d tuples ",total);
	}

	if (silent) {
		if (verbose) printf("\n");
		return;
	}

	for (k = 0; k < g; k++) {

		n = PQntuplesGroup(p, k);
		m = PQnfieldsGroup(p, k);
		
		if ( m > 0 ) {  /* only print tuples with at least 1 field. */
		
			if (verbose) {
				/* Print out the attribute names */
				printf("(");
				for (i=0; i < m; i++) 
					printf(" %s",PQfnameGroup(p,k,i));
				printf(" ):\n");
			}
		
			/* Print out the tuples */
			for (i = 0; i < n; i++) {
				for(j = 0; j < m; j++) {
					print_value(PQfnameGroup(p,k,j),
						    PQgetvalue(p,t+i,j),
						    PQftype(p,t+i,j));
				}
				printf(separator);
			}
			t += n;
		}
	}
}

/*
 * process a query
 */

do_query(query)
char *query;
{
	bool done = false;
	int nqueries = 0;
	char *ret_string;
	char *result;
	char ret_val;
	int len;

	if ((len = strlen(query)) == 0)
		return;

	if (len + strlen(querybuffer) > MAXQUERYSIZE) {
		fprintf (stderr, "error: query buffer overflow\n");
		querybuffer[0] = '\0';
		query_continues = 0;
		return;
	}
	
	strcat(querybuffer, query);
	if (query[len-1] == '\\') {
		querybuffer[strlen(querybuffer)-1] = ' ';
		query_continues = 1;
		return;
	}

#ifdef DEBUG
	printf("query: %s\n", querybuffer);
#endif

	result = PQexec(querybuffer);
	querybuffer[0] = '\0';
	query_continues = 0;

	while (!done) {

		ret_val    = result[0];
		ret_string = &result[1];

#ifdef DEBUG
		printf("query result: %s\n", result);
#endif

		switch (ret_val) {
		case 'A':
		case 'P':
			print_tuples(ret_string);
			PQclear(ret_string);
			result = PQexec(" ");
			nqueries++;
			break;
		case 'E':
			PQreset();
			done = true;
			fputs("spog: detected a fatal error, exiting...\n",
			      stderr),
			exit(2);	/* XXX */
		case 'R':
			PQreset();
			done = true;
			break;
		case 'C':
			printf("%s successful\n", ret_string);
			result = PQexec(" ");
			nqueries++;
			break;
		case 'I':
			PQFlushI(nqueries - 1);
			done = true;
			break;
		default:
			fprintf(stderr, "error: type %c: %s\n", 
				ret_val, ret_string);
			break;
		}
	}
}

