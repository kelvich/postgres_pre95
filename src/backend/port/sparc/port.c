#define RcsId(a)

RcsId("$Header$");

/*
 * Hackety-hack for Sparc compiler error.
 */

void
sparc_bug_set_outerjoincost(p, val)

char *p;
long val;

{
     set_outerjoincost(p, val);
}
