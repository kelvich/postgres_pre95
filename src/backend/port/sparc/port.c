/*
 *   FILE
 *	port.c
 *
 *   DESCRIPTION
 *	SunOS4-specific routines
 *
 *   INTERFACE ROUTINES
 *	sparc_bug_set_outerjoincost
 *
 *   NOTES
 *
 *   IDENTIFICATION
 *	$Header$
 */
#include "nodes/relation.h"

RcsId("$Header$");

/*
 * Hackety-hack for Sparc compiler error.
 *
 * See planner/path/joinpath.c
 */
void
sparc_bug_set_outerjoincost(p, val)
    char *p;
    long val;
{
    set_outerjoincost((Path)p, (Cost)val);
}
