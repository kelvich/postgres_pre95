/* ----------------------------------------------------------------
 *   FILE
 *	portalbuf.c
 *	
 *   DESCRIPTION
 *	portal buffer support routines for src/libpq/portal.c
 *
 *   INTERFACE ROUTINES
 *	pq_alloc 	- allocate memory for libpq routines
 *	pq_free 	- free memory for libpq routines
 *	addPortal 	- Allocate a new portal buffer
 *	addGroup 	- Add a new tuple group to the portal
 *	addTypes 	- Allocate n type blocks
 *	addTuples 	- Allocate a tuple block
 *	addTuple 	- Allocate a tuple of n fields (attributes)
 *	addValues 	- Allocate n bytes for a value
 *	addPortalEntry 	- Allocate a portal entry
 *	freePortalEntry - Free a portal entry in the portal table
 *	freeTypes 	- Free up the space used by a portal 
 *	freeTuples 	- free space used by tuple block
 *	freeGroup 	- free space used by group, types and tuples
 *	freePortal 	- free space used by portal and portal's group
 *	
 *   NOTES
 *	These functions may be used by both frontend routines which
 *	communicate with a backend or by user-defined functions which
 *	are compiled or dynamically loaded into a backend.
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#include <sys/types.h>

#include "tmp/c.h"

RcsId ("$Header$");

#include "tmp/libpq.h"
#include "utils/exc.h"

PortalEntry *portals[MAXPORTALS];

/* --------------------------------
 *	pq_alloc - allocate memory for libpq routines
 *
 *	remember: palloc() in the backend uses the postgres MemoryContext
 *	library and palloc() in the frontend (pqstubs.c) just calls malloc().
 * --------------------------------
 */
caddr_t
pq_alloc(size)
    size_t size;
{
    extern	caddr_t	malloc();
    caddr_t 	addr;

    if (size <= 0)
	libpq_raise(MemoryError, form("Invalid argument to pg_alloc()."));

    addr = (caddr_t) palloc(size);
    if (addr == NULL)
	libpq_raise(MemoryError, form("Cannot Allocate space."));

    return (addr);
}

/* --------------------------------
 *	pq_free - free memory for libpq routines
 *
 *	remember: pfree() in the backend uses the postgres MemoryContext
 *	library and pfree() in the frontend (pqstubs.c) just calls free().
 * --------------------------------
 */
void
pq_free(pointer)
    caddr_t pointer;
{
    pfree(pointer);
}

/* --------------------------------
 *	addPortal - Allocate a new portal buffer
 * --------------------------------
 */
PortalBuffer *
addPortal()
{
    PortalBuffer *portal;
    
    portal = (PortalBuffer *)
	pq_alloc(sizeof (PortalBuffer));
    
    portal->rule_p = 0;
    portal->no_tuples = 0;
    portal->no_groups = 0;
    portal->groups = NULL;

    return (portal);
}

/* --------------------------------
 *	addGroup - Add a new tuple group to the portal
 * --------------------------------
 */
GroupBuffer *
addGroup(portal)
    PortalBuffer *portal;
{
    GroupBuffer *group, *group1;

    group = (GroupBuffer *)
	pq_alloc(sizeof (GroupBuffer));
    
    /* Initialize the new group buffer. */
    group->no_tuples  = 0;
    group->no_fields = 0;
    group->types = NULL;
    group->tuples = NULL;
    group->next = NULL;
    
    if ((group1 = portal->groups) == NULL)
	portal->groups = group;
    else {
	while (group1->next != NULL) 
	    group1 = group1->next;
	group1->next = group;
    }
    
    return (group);
}

/* --------------------------------
 *	addTypes - Allocate n type blocks
 * --------------------------------
 */
TypeBlock *
addTypes(n)
    int n;
{
    TypeBlock *types;

    types = (TypeBlock *)
	pq_alloc(n * sizeof (TypeBlock));

    return (types);
}

/* --------------------------------
 *	addTuples - Allocate a tuple block
 * --------------------------------
 */
/*  */
TupleBlock *
addTuples()
{
    TupleBlock *tuples;

    tuples = (TupleBlock *)
	pq_alloc(sizeof (TupleBlock));
    
    tuples->next = NULL;

    return (tuples);
}

/* --------------------------------
 *	addTuple - Allocate a tuple of n fields (attributes)
 * --------------------------------
 */
char **
addTuple(n)
{
    return (char **)
	pq_alloc(n * sizeof (char *));
}

/* --------------------------------
 *	addValues - Allocate n bytes for a value
 * --------------------------------
 */
char *
addValues(n)
    int n;
{
    return
	pq_alloc(n);
}

/* --------------------------------
 *	addPortalEntry - Allocate a portal entry
 * --------------------------------
 */
PortalEntry *
addPortalEntry()
{
    return (PortalEntry *)
	pq_alloc (sizeof (PortalEntry));
}

/* --------------------------------
 *	freePortalEntry - Free a portal entry in the portal table
 *	the portal is freed separately.
 * --------------------------------
 */
void
freePortalEntry(i)
    int i;
{
    pq_free (portals[i]);
    portals[i] = NULL;
}


/* --------------------------------
 *	freeTypes - Free up the space used by a portal 
 * --------------------------------
 */

void 
freeTypes(types)
    TypeBlock *types;
{
    pq_free(types);
}

/* --------------------------------
 *	freeTuples - free space used by tuple block
 * --------------------------------
 */
void
freeTuples(tuples, no_tuples, no_fields)
    TupleBlock *tuples;
    int		no_tuples;
    int		no_fields;
{
    int i, j;
    
    if (no_tuples > TupleBlockSize) {
	freeTuples (tuples->next, no_tuples - TupleBlockSize, no_fields);
	no_tuples = TupleBlockSize;
    }
    
    /* For each tuple, free all its attribute values. */
    for (i = 0; i < no_tuples; i++) {
	for (j = 0; j < no_fields; j++)
	    if (tuples->values[i][j] != NULL)
		pq_free(tuples->values[i][j]);
	pq_free(tuples->values[i]);
    }
    
    pq_free(tuples);
}

/* --------------------------------
 *	freeGroup - free space used by group, types and tuples
 * --------------------------------
 */
void
freeGroup(group)
    GroupBuffer *group;
{
    if (group->next != NULL)
	freeGroup(group->next);

    if (group->types != NULL)
	freeTypes(group->types);

    if (group->tuples != NULL)
	freeTuples(group->tuples, group->no_tuples,group->no_fields);

    pq_free(group);
}

/* --------------------------------
 *	freePortal - free space used by portal and portal's group
 * --------------------------------
 */
void
freePortal(portal)
    PortalBuffer *portal;
{
    if (portal->groups != NULL)
	freeGroup(portal->groups);
    
    pq_free(portal);
}
