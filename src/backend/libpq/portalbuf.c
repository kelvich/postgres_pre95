/* ----------------------------------------------------------------
 *   FILE
 *	portalbuf.c
 *	
 *   DESCRIPTION
 *	portal buffer support routines for src/libpq/portal.c
 *
 *   INTERFACE ROUTINES
 *	pbuf_alloc 	  - allocate memory for libpq routines
 *	pbuf_free 	  - free memory for libpq routines
 *	pbuf_addPortal 	  - Allocate a new portal buffer
 *	pbuf_addGroup 	  - Add a new tuple group to the portal
 *	pbuf_addTypes 	  - Allocate n type blocks
 *	pbuf_addTuples 	  - Allocate a tuple block
 *	pbuf_addTuple 	  - Allocate a tuple of n fields (attributes)
 *	pbuf_addValues 	  - Allocate n bytes for a value
 *	pbuf_addEntry 	  - Allocate a portal entry
 *	pbuf_freeEntry    - Free a portal entry in the portal table
 *	pbuf_freeTypes 	  - Free up the space used by a portal 
 *	pbuf_freeTuples   - free space used by tuple block
 *	pbuf_freeGroup 	  - free space used by group, types and tuples
 *	pbuf_freePortal   - free space used by portal and portal's group
 *	pbuf_getIndex 	  - Return the index of the portal entry
 *	pbuf_setup 	  - Set up a portal for dumping data
 *	pbuf_close 	  - Close a portal, remove it from the portal table
 *	pbuf_findGroup 	  - Return group given the group_index
 *	pbuf_findFnumber  - Return field index of a given field within a group
 *	pbuf_findFname 	  - Find the field name given the field index
 *	pbuf_checkFnumber - signal an error if field number is out of bounds
 *
 *   NOTES
 *	These functions may be used by both frontend routines which
 *	communicate with a backend or by user-defined functions which
 *	are compiled or dynamically loaded into a backend.
 *
 *	the portals[] array should be organized as a hash table for
 *	quick portal-by-name lookup.
 *
 *	Do not confuse "PortalEntry" (or "PortalBuffer") with "Portal"
 *	see utils/mmgr/portalmem.c for why. -cim 2/22/91
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#include <sys/types.h>

#include "tmp/c.h"

RcsId ("$Header$");

#include "tmp/simplelists.h"
#include "tmp/libpq.h"
#include "utils/exc.h"

PortalEntry *portals[MAXPORTALS];

/* --------------------------------
 *	pbuf_alloc - allocate memory for portal buffers
 *
 *	remember: palloc() in the backend uses the postgres MemoryContext
 *	library and palloc() in the frontend (fe-pqstubs.c) calls malloc().
 * --------------------------------
 */
caddr_t
pbuf_alloc(size)
    size_t size;
{
    caddr_t 	addr;

    if (size <= 0)
	libpq_raise(&MemoryError, form((int)"Invalid argument to pg_alloc()."));

    addr = (caddr_t) palloc(size);
    if (addr == NULL)
	libpq_raise(&MemoryError, form((int)"Cannot Allocate space."));

    return (addr);
}

/* --------------------------------
 *	pbuf_free - free memory for portal buffers
 *
 *	remember: pfree() in the backend uses the postgres MemoryContext
 *	library and pfree() in the frontend (fe-pqstubs.c) calls free().
 * --------------------------------
 */
void
pbuf_free(pointer)
    caddr_t pointer;
{
    pfree(pointer);
}

/* --------------------------------
 *	pbuf_addPortal - Allocate a new portal buffer
 * --------------------------------
 */
PortalBuffer *
pbuf_addPortal()
{
    PortalBuffer *portal;
    
    portal = (PortalBuffer *)
	pbuf_alloc(sizeof (PortalBuffer));
    
    portal->rule_p = 0;
    portal->no_tuples = 0;
    portal->no_groups = 0;
    portal->groups = NULL;

    return (portal);
}

/* --------------------------------
 *	pbuf_addGroup - Add a new tuple group to the portal
 * --------------------------------
 */
GroupBuffer *
pbuf_addGroup(portal)
    PortalBuffer *portal;
{
    GroupBuffer *group, *group1;

    group = (GroupBuffer *)
	pbuf_alloc(sizeof (GroupBuffer));
    
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
 *	pbuf_addTypes - Allocate n type blocks
 * --------------------------------
 */
TypeBlock *
pbuf_addTypes(n)
    int n;
{
    TypeBlock *types;

    types = (TypeBlock *)
	pbuf_alloc(n * sizeof (TypeBlock));

    return (types);
}

/* --------------------------------
 *	pbuf_addTuples - Allocate a tuple block
 * --------------------------------
 */
/*  */
TupleBlock *
pbuf_addTuples()
{
    TupleBlock *tuples;

    tuples = (TupleBlock *)
	pbuf_alloc(sizeof (TupleBlock));
    
    tuples->next = NULL;

    return (tuples);
}

/* --------------------------------
 *	pbuf_addTuple - Allocate a tuple of n fields (attributes)
 * --------------------------------
 */
char **
pbuf_addTuple(n)
{
    return (char **)
	pbuf_alloc(n * sizeof (char *));
}

/* --------------------------------
 *	pbuf_addValues - Allocate n bytes for a value
 * --------------------------------
 */
char *
pbuf_addValues(n)
    int n;
{
    return
	pbuf_alloc(n);
}

/* --------------------------------
 *	pbuf_addEntry - Allocate a portal entry
 * --------------------------------
 */
PortalEntry *
pbuf_addEntry()
{
    return (PortalEntry *)
	pbuf_alloc (sizeof (PortalEntry));
}

/* --------------------------------
 *	pbuf_freeEntry - Free a portal entry in the portal table
 *	the portal is freed separately.
 * --------------------------------
 */
void
pbuf_freeEntry(i)
    int i;
{
    pbuf_free ((caddr_t)portals[i]);
    portals[i] = NULL;
}


/* --------------------------------
 *	pbuf_freeTypes - Free up the space used by a portal 
 * --------------------------------
 */

void 
pbuf_freeTypes(types)
    TypeBlock *types;
{
    pbuf_free((caddr_t)types);
}

/* --------------------------------
 *	pbuf_freeTuples - free space used by tuple block
 * --------------------------------
 */
void
pbuf_freeTuples(tuples, no_tuples, no_fields)
    TupleBlock *tuples;
    int		no_tuples;
    int		no_fields;
{
    int i, j;
    
    if (no_tuples > TupleBlockSize) {
	pbuf_freeTuples (tuples->next, no_tuples - TupleBlockSize, no_fields);
	no_tuples = TupleBlockSize;
    }
    
    /* For each tuple, free all its attribute values. */
    for (i = 0; i < no_tuples; i++) {
	for (j = 0; j < no_fields; j++)
	    if (tuples->values[i][j] != NULL)
		pbuf_free((caddr_t)tuples->values[i][j]);
	pbuf_free((caddr_t)tuples->values[i]);
    }
    
    pbuf_free((caddr_t)tuples);
}

/* --------------------------------
 *	pbuf_freeGroup - free space used by group, types and tuples
 * --------------------------------
 */
void
pbuf_freeGroup(group)
    GroupBuffer *group;
{
    if (group->next != NULL)
	pbuf_freeGroup(group->next);

    if (group->types != NULL)
	pbuf_freeTypes(group->types);

    if (group->tuples != NULL)
	pbuf_freeTuples(group->tuples, group->no_tuples,group->no_fields);

    pbuf_free((caddr_t)group);
}

/* --------------------------------
 *	pbuf_freePortal - free space used by portal and portal's group
 * --------------------------------
 */
void
pbuf_freePortal(portal)
    PortalBuffer *portal;
{
    if (portal->groups != NULL)
	pbuf_freeGroup(portal->groups);
    
    pbuf_free((caddr_t)portal);
}

/* --------------------------------
 *	pbuf_getIndex - Return the index of the portal entry
 * 	note: portals[] maps portal names to portal buffers.
 * --------------------------------
 */
int 
pbuf_getIndex(pname)
    char *pname;
{
    int i;

    for (i = 0; i < MAXPORTALS; i++) 
	if (portals[i] != NULL && strcmp(portals[i]->name, pname) == 0)
	    return i;
    
    return (-1);
}

/* --------------------------------
 *	pbuf_setportalname - assign a user given name to a portal
 * --------------------------------
 */

void
pbuf_setportalinfo(entry, pname)
    PortalEntry *entry;
    char *pname;
{
    if (entry)
	strncpy(entry->name, pname, PortalNameLength-1);
}

/* --------------------------------
 *	pbuf_setup - Set up a portal for dumping data
 * --------------------------------
 */
PortalEntry *
pbuf_setup(pname)
    char *pname;
{
    int i;

    /* If a portal with the same name already exists, close it. */
    /* else look for an empty entry in the portal table. */
    if ((i = pbuf_getIndex(pname)) != -1) 
	pbuf_freePortal(portals[i]->portal);
    else {
	for (i = 0; i < MAXPORTALS; i++)
	    if (portals[i] == NULL)
		break;
	/* If the portal table is full, signal an error. */
	if (i >= MAXPORTALS) 
	    libpq_raise(&PortalError, form((int)"Portal Table overflows!"));
	
	portals[i] = pbuf_addEntry();
	strncpy(portals[i]->name, pname, PortalNameLength-1);
    }
    portals[i]->portal = pbuf_addPortal();
    portals[i]->portalcxt = NULL;
    portals[i]->result = NULL;
    
    return (PortalEntry *)
	portals[i];
}

/* --------------------------------
 *	pbuf_close - Close a portal, remove it from the portal table
 *			and free up the space
 * --------------------------------
 */
void
pbuf_close(pname)
    char *pname;
{
    int i;

    if ((i = pbuf_getIndex(pname)) == -1) 
	libpq_raise(&PortalError, form((int)"Portal %s does not exist.", pname));

    pbuf_freePortal(portals[i]->portal);
    pbuf_freeEntry(i);
}

/* --------------------------------
 *	pbuf_findGroup - Return the group given the group_index
 * --------------------------------
 */
GroupBuffer *
pbuf_findGroup(portal, group_index)
    PortalBuffer *portal;
    int 	 group_index;
{
    GroupBuffer *group;

    group = portal->groups;
    while (group_index > 0 && group != NULL) {
	group = group->next;
	group_index--;
    }

    if (group == NULL)
	libpq_raise(&PortalError, 
		    form((int)"Group index %d out of bound.", group_index));

    return (group);
}

/* --------------------------------
 *	pbuf_findFnumber - Return the field index of a given field within a group
 * --------------------------------
 */
pbuf_findFnumber(group, field_name)
    GroupBuffer *group;
    char	*field_name;
{	
    TypeBlock *types;
    int i;

    types = group->types;

    for (i = 0; i < group->no_fields; i++, types++) 
	if (strcmp(types->name, field_name) == 0)
	    return (i);
	
    libpq_raise(&PortalError, 
		form((int)"Field-name %s does not exist.", field_name));
}

/* --------------------------------
 *	pbuf_checkFnumber - signal an error if field number is out of bounds
 * --------------------------------
 */
void
pbuf_checkFnumber(group, field_number)
    GroupBuffer *group;
    int		 field_number;
{
    if (field_number < 0 || field_number >= group->no_fields)
	libpq_raise(&PortalError, 
		    form((int)"Field number %d out of bound.", field_number));
}

/* --------------------------------
 *	pbuf_findFname - Find the field name given the field index
 * --------------------------------
 */
char *
pbuf_findFname(group, field_number)
    GroupBuffer *group;
    int		field_number;
{
     pbuf_checkFnumber(group, field_number);
     return
	 (group->types + field_number)->name;
}

