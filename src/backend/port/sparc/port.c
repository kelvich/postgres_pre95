/*
 * Hackety-hack for Sparc compiler error.
 */

#include "nodes/relation.h"

RcsId("$Header$");

void
sparc_bug_set_outerjoincost(p, val)

char *p;
long val;

{
     set_outerjoincost((Path)p, (Cost)val);
}
