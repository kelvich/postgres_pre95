/*
 * Testing of asynchronous notification interface.
 *
 * Do the following at the monitor:
 *
 *    * create asynctest1 (i = int4) \g
 *    * create asynctest1a (i = int4) \g
 *
 *    * define rule r1 is on append to asynctest1 do
 *      [append asynctest1a (i = new.i)
 *       notify asynctest1a] \g
 *
 *
 * Then start up this process.
 *
 *    * append asynctest1 (i = 10) \g
 *
 * The value i = 10 should be printed by this process.
 *
 */

#include <tmp/simplelists.h>
#include <tmp/libpq.h>
#include <tmp/libpq-fe.h>
#include <tmp/postgres.h>

extern int PQAsyncNotifyWaiting;

void main() {
  PQNotifyList *l;
  PortalBuffer *portalBuf;
  char *res;
  int ngroups, tupno, grpno, ntups, nflds;

  PQsetdb("aportaldb");

  printf("aportal: listen asynctest1a\n");
  (void) fflush(stdout);
  PQexec("listen asynctest1a");

  while (1) {
    res = PQexec(" ");
    if (*res != 'I') {
      printf("Unexpected result from a null query --> %s", res);
      (void) fflush(stdout);
      PQfinish();
      exit(1);
    }
    if (PQAsyncNotifyWaiting) {
      PQAsyncNotifyWaiting = 0;
      for (l = PQnotifies() ; l != NULL ; l = PQnotifies()) {
	PQremoveNotify(l);
	printf("Async. notification on relation %s, our backend pid is %d\n",
	       l->relname, l->be_pid);
	(void) fflush(stdout);
	printf("aportal: retrieve (asynctest1a.i)\n");
	(void) fflush(stdout);
	res = PQexec("retrieve (asynctest1a.i)");

	if (*res != 'P') {
	  fprintf(stderr, "%s\nno portal", ++res);
	  PQfinish();
	  exit(1);
	}

	portalBuf = PQparray(++res);
	ngroups = PQngroups(portalBuf);
	for (grpno = 0 ; grpno < ngroups ; grpno++) {
	  ntups = PQntuplesGroup(portalBuf, grpno);
	  nflds = PQnfieldsGroup(portalBuf, grpno);
	  if (nflds != 1) {
	    fprintf(stderr, "expected 1 attributes, got %d\n", nflds);
	    PQfinish();
	    exit(1);
	  }
	  for (tupno = 0 ; tupno < ntups ; tupno++) {
	    printf("i = %s\n", PQgetvalue(portalBuf, tupno, 0));
	    (void) fflush(stdout);
	  }
	}
      }
      PQfinish();
      printf("aportal: done.\n");
      (void) fflush(stdout);
      exit(0);
    }
  sleep(1);
  }
}
