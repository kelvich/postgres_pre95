#include <stdio.h>
#include <sys/file.h>

#include "tmp/libpq-fs.h"

int errs;
void copy();
extern char	*getenv();
extern char	*PQexec();

void usage();

void main(argc,argv)
     int argc;       
     char *argv[];
{
    int infd;
    int cnt, total = 0;
    int blen;
    int i;
    char *res;
    char *dbname;
    int dflag,ch;
    extern int optind;
    extern char *optarg;
    extern char *PQhost, *PQport;

    dflag = 0;

	while ((ch = getopt(argc, argv, "H:P:D:")) != EOF)
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
	    case '?':
	    default:
	      usage();
	  }

    if (!dflag) {
	if ((dbname = getenv("DATABASE")) == (char *) NULL) {
	    usage();
	}
	PQsetdb(dbname);
    }

    if ((argc-optind) == 0)	{
	copy(0,1);
	exit(0);
    }

    res = PQexec("begin");
    for (i=1;i<argc;i++) {
         if (!strcmp(argv[i],"-")) {
	    copy(0,1);
	    continue;
	 }
         infd = p_open(argv[i],INV_READ|O_RDONLY);
	 if (infd < 0) {
		errs = 1;
		continue;
	 }
	 if (pgcopy(infd,1) < 0) errs = 1;
         p_close(infd);
    }

    res = PQexec("end");
    PQfinish();
    exit(0);
}

void copy(i,o)
{
    char buf[2048];
    int n;
    while ((n = read(i,buf,sizeof(buf))) > 0) {
	write(o,buf,n);
    }
}

pgcopy(i,o)
{
    char buf[2048];
    int n;
    while ((n = p_read(i,buf,sizeof(buf))) > 0) {
	write(o,buf,n);
    }
    if (n < 0)
      return -1;
    else 
      return 0;
}

void usage() 
{
    fprintf(stderr, "no database specified with -D option or in env var DATABASE\n");
    fflush(stderr);
    exit (1);
}
