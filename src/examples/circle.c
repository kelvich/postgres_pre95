/*
 * circle example from chapter 12
 *
 * $Header$
 */

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "tmp/c.h"           /* (always)                 */
#include "utils/geo-decls.h" /* for POINT declaration    */
#include "utils/palloc.h"    /* for palloc() declaration */

typedef struct {
    POINT  center;
    double radius;
} CIRCLE;

#define LDELIM '('
#define RDELIM ')'
#define NARGS  3

CIRCLE *
circle_in(str)
    char   *str;
{
    char   *p, *coord[NARGS];
    int    i;
    CIRCLE *result;

    if (str == (char *) NULL) 
	return((CIRCLE *) NULL);

    for (i = 0, p = str;
         *p && i < NARGS && *p != RDELIM;
         p++)
    {
        if (*p == ',' || (*p == LDELIM && !i))
            coord[i++] = p + 1;
    }

    if (i < NARGS - 1) 
	return((CIRCLE *) NULL);

    result = (CIRCLE *) palloc(sizeof(CIRCLE));

    result->center.x = atof(coord[0]);
    result->center.y = atof(coord[1]);
    result->radius = atof(coord[2]);

    return(result);
}

char *
circle_out(circle)
    CIRCLE *circle;
{
    char   *result;

    if (circle == (CIRCLE *) NULL)
	return((char *) NULL);

    result = (char *) palloc(60);

    sprintf(result, "(%g, %g, %g)",
            circle->center.x, circle->center.y,
            circle->radius);

    return(result);
}

int
eq_area_circle(circle1, circle2)
    CIRCLE *circle1, *circle2;
{
    if (circle1 == (CIRCLE *) NULL)
	return(circle2 == (CIRCLE *) NULL);
    if (circle2 == (CIRCLE *) NULL)
	return(0);
    return(circle1->radius == circle2->radius);
}
