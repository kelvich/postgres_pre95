#include <stdio.h>
#include <math.h>
#include "dl.h"

double alpha= 2.094395;

main (argc, argv)
     int argc;
     char *argv[];
{
    void *handle;
    void (*func) ();
#if 0
    double dummy,alpha;
#endif

    (void) dl_init (argv[0]);
    dl_setLibraries("/usr/lib/libm_G0.a");

    if ((handle=dl_open("jumboobj.o", DL_NOW)) == NULL) {
	fprintf(stderr, "%s\n", dl_error());
	exit(1);
    }

    /*
     * trigo
     */
#if 0
    dummy = sin(0); dummy = cos(0); dummy = tan(0);
    dummy = fabs(1);

    printf("Enter alpha ");scanf("%lf",&alpha);
    printf("Loading file\n");
#endif

    printf("*** TRIGO ***\n");
    if (!(func= (void(*)()) dl_sym(handle, "trigo"))) {
	fprintf(stderr, "trigo: %s\n", dl_error());
    }else {
	func((double)alpha);
    }

    /*
     * testanotherbug
     */
    printf("*** TESTANOTHERBUG ***\n");
    if (!(func = dl_sym(handle, "testanotherbug"))) {
	fprintf(stderr, "testanotherbug: %s\n", dl_error());
    }else {
	(*func)(0);
    }

    /*
     * teststaticbug
     */
    printf("*** TESTSTATICBUG ***\n");
    if (!(func = dl_sym(handle, "teststaticbug"))) {
	fprintf(stderr, "teststaticbug: %s\n", dl_error());
    }else {
	(*func)(0);
    }

    /*
     * testswitchbug
     */
    printf("*** TESTSWITCHBUG ***\n");
    if (!(func = dl_sym(handle, "testswitchbug"))) {
	fprintf(stderr, "testswitchbug: %s\n", dl_error());
    }else {
	(*func)(3);
	(*func)(2);
	(*func)(1);
	(*func)(0);
    }

    printf("*** DBL ***\n");
    if (!(func = dl_sym(handle, "dbl"))) {
	fprintf(stderr, "dbl: %s\n", dl_error());
    }else {
	(*func)();
    }

    dl_close(handle);
}
