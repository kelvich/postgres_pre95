/*
 * $Header$
 *
 * Test program for the binary portal interface.
 * 
 * Create a test database and do the following at the monitor:
 *
 *	* create iportaltest (i = int4, d = float4, p = polygon)\eg
 *	* append iportaltest (i = 1, d = 3.567,
 *	  p = "(3.0,4.0,1.0,2.0)"::polygon)\eg
 *	* append iportaltest (i = 2, d = 89.05,
 *	  p = "(4.0,3.0,2.0,1.0)"::polygon)\eg
 *
 * adding as many tuples as desired.
 *
 * Start up this program.  The contents of class "iportaltest" should be
 * printed, e.g.:

tuple 0: got
         i=(4 bytes) 1,
         d=(4 bytes) 3.567000,
         p=(72 bytes) 2 points,
                 boundbox=(hi=3.000000,4.000000 / lo=1.000000,2.000000)
tuple 1: got
         i=(4 bytes) 2,
         d=(4 bytes) 89.05000,
         p=(72 bytes) 2 points,
                 boundbox=(hi=4.000000,3.000000 / lo=2.000000,1.000000)

 */

#include "tmp/simplelists.h"
#include "tmp/libpq.h"
#include "utils/geo-decls.h"

void
exit_error()
{
    (void) PQexec("close regressiportal");
    (void) PQexec("end");
    PQfinish();
    exit(1);
}

main(argc, argv)
    int argc;
    char **argv;
{
    PortalBuffer *portalbuf;
    char *res;
    int ngroups, tupno, grpno, ntups, nflds;

    if (argc != 2) {
	fprintf(stderr, "Usage: %s database_name\n", argv[0]);
	exit(1);
    }

    PQsetdb(argv[1]);
    (void) PQexec("begin");
    res = (char *) PQexec("retrieve iportal regressiportal (iportaltest.all)");
    if (!res) {
	fprintf(stderr, "\nError: PQexec returned NULL from retrieve\n");
	exit_error();
    }
    if (*res == 'E') {
	fprintf(stderr, "\nError: \"%s\"\n",++res);
	exit_error();
    }
    res = (char *) PQexec("fetch all in regressiportal");
    if (!res) {
	fprintf(stderr, "\nError: PQexec returned NULL from fetch\n");
	exit_error();
    }
    if (*res != 'P') {
	fprintf(stderr,"\nError: no portal ID returned\n");
	exit_error();
    }
    /* get tuples in relation */
    portalbuf = PQparray(++res);
    ngroups = PQngroups(portalbuf);
    for (grpno = 0; grpno < ngroups; grpno++) {
	ntups = PQntuplesGroup(portalbuf, grpno);
	if ((nflds = PQnfieldsGroup(portalbuf, grpno)) != 3) {
	    fprintf(stderr, "\nError: expected 3 attributes, got %d\n", nflds);
	    exit_error();
	}
	for (tupno = 0; tupno < ntups; tupno++) {
	    int *ival;		/* 4 bytes */
	    float *fval;	/* 4 bytes */
	    unsigned plen;
	    POLYGON *pval;

	    ival = (int *) PQgetvalue(portalbuf, tupno, 0);
	    fval = (float *) PQgetvalue(portalbuf, tupno, 1);
	    plen = PQgetlength(portalbuf, tupno, 2);
	    if (!(pval = (POLYGON *) palloc(plen + sizeof(long)))) {
		fprintf(stderr, "\nError: palloc returned zero bytes\n");
		exit_error();
	    }
	    pval->size = plen + sizeof(long);
	    bcopy(PQgetvalue(portalbuf, tupno, 2), (char *) &pval->npts, plen);
	    printf ("tuple %d: got\n\
\t i=(%d bytes) %d,\n\
\t d=(%d bytes) %f,\n\
\t p=(%d bytes) %d points,\n\
\t\t boundbox=(hi=%f,%f / lo=%f,%f)\n",
		    tupno,
		    PQgetlength(portalbuf, tupno, 0),
		    *ival,
		    PQgetlength(portalbuf, tupno, 1),
		    *fval,
		    PQgetlength(portalbuf, tupno, 2),
		    pval->npts,
		    pval->boundbox.xh,
		    pval->boundbox.yh,
		    pval->boundbox.xl,
		    pval->boundbox.yl);
	}
    }
    (void) PQexec("close regressiportal");
    (void) PQexec("end");
    PQfinish();
    exit(0);
}
