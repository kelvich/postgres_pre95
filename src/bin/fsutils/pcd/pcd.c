#ifndef lint
static	char rcsid[] = "$Id$";
#endif
/*
 * cd
 */
#include <stdio.h>
#include <sys/param.h>
#include "tmp/libpq-fs.h"

extern char *getwd();
extern char *getenv();
void usage();

main(argc,argv)
char *argv[];
int argc;
{
	char pathname[MAXPATHLEN + 1];
	int pflag, dflag, ch;
	char *dbname;
	extern int optind;
	extern char *optarg;
	extern char *PQhost, *PQport;

	pflag = dflag = 0;
	while ((ch = getopt(argc, argv, "H:P:D:p")) != EOF)
	  switch(ch) {
	    case 'H':
	      PQhost = optarg;
	      break;
	    case 'P':
	      PQport = optarg;
	      break;
	    case 'D':
	      PQsetdb(optarg);
	      dflag = 1;
	      break;
	    case 'p':
	      pflag = 1;
	      break;
	    case '?':
	    default:
	      usage();
	  }
	
	if (!dflag) {
	    if ((dbname = getenv("DATABASE")) == (char *) NULL) {
		fprintf(stderr, "no database specified with -D option or in env var DATABASE\n");
		fflush(stderr);
		exit (1);
	    }
	    PQsetdb(dbname);
	}
	{
	    extern int p_attr_caching;
	    p_attr_caching = 0;
	}

	if ((argc-optind) > 1) {
	    usage();
	} else if ((argc-optind) == 0) {
		argv[optind] = "/";
	}
	
	if (p_chdir(argv[optind]) < 0) {
		fprintf(stderr, "pcd: bad pathname %s\n", argv[optind]);
		exit(1);
	}
	p_getwd(pathname);
	printf("setenv PFCWD %s\n",pathname);
	PQfinish();
	exit(0);
	/* NOTREACHED */
}

void usage()
{
    fprintf(stderr,"wrong # of arguments.\nusage: pcd directory\n");
    exit(1);
}
