#include <stdio.h>
#include <sys/file.h>

#include "tmp/libpq-fs.h"

int errs;
void copy();
extern char	*getenv();
extern char	*PQexec();

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

    if ((dbname = getenv("DATABASE")) == (char *) NULL) {
	fprintf(stderr, "no database specified in env var DATABASE\n");
	fflush(stderr);
	exit (1);
    }

    PQsetdb(dbname);

    if (argc == 1)	{
	copy(0,1);
	exit(0);
    }

    res = PQexec("begin");
    for (i=1;i<argc;i++) {
         if (!strcmp(argv[i],"-")) {
	    copy(0,1);
	    continue;
	 }
         infd = p_open(argv[i],O_RDONLY);
	 if (infd < 0) {
		errs = 1;
		continue;
	 }
	 if (pgcopy(infd,1) < 0) errs = 1;
         p_close(infd);
    }

    res = PQexec("end");
    PQfinish();
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
