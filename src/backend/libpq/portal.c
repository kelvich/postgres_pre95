/*
 * portal.c --
 *	Handling portals.
 */

#include <string.h>

#include "tmp/c.h"
#include "tmp/libpq.h"
#include "utils/exc.h"

RcsId ("$Header$");

/* 
 * portals[] --
 *	maps portal names to portal buffers.
 */

/* Return the index of the portal entry. */
int 
get_portal_index (pname)
	char *pname;
{
    int i;

    for (i = 0; i < MAXPORTALS; i++) 
	if (portals[i] != NULL && strcmp (portals[i]->name, pname) == 0)
	    return (i);
    return (-1);
}

/* Set up a portal for dumping data. */
PortalBuffer *
portal_setup (pname)
	char *pname;
{
    int i;

    /* If a portal with the same name already exists, close it. */
    if ((i = get_portal_index (pname)) != -1) 
	freePortal (portals[i]->portal);
    /* Look for an empty entry in the portal table. */
    else {
	for (i = 0; i < MAXPORTALS; i++)
	    if (portals[i] == NULL)
		break;
	/* If the portal table is full, signal an error. */
	if (i >= MAXPORTALS) 
	    libpq_raise(PortalError, form("Portal Table overflows!"));
	portals[i] = addPortalEntry ();
	strcpy (portals[i]->name, pname);
    }
    portals[i]->portal = addPortal ();

    return (portals[i]->portal);
}

/* Close a portal, remove it from the portal table and free up the space. */
void
portal_close (pname)
	char *pname;
{
    int i;

    if ((i = get_portal_index (pname)) == -1) 
	libpq_raise(PortalError, form("Portal %s does not exist.", pname));

    freePortal (portals[i]->portal);
    freePortalEntry (i);
}

/* 
 * Return the number of open portals. 
 * If rule_p, only return asynchronized portals. 
 */
int
PQnportals (rule_p)
	int rule_p;
{
    int i, n = 0;
    
    for (i = 0; i < MAXPORTALS; i++) 
	if (portals[i] != NULL)
	    if (!rule_p || portals[i]->portal->rule_p)
		n++;
    return (n);
}

/* 
 * Return all the portal names.
 * If rule_p, only return asynchronized portals. 
 */
void
PQpnames (pnames, rule_p)
	char *pnames[MAXPORTALS];
{
    int i;

    for (i = 0; i < MAXPORTALS; i++)
	if (portals[i] != NULL) {
	    if (!rule_p || portals[i]->portal->rule_p) 
		strcpy (pnames[i], portals[i]->name);
	    else pnames[i] = NULL;
	}
	else 
	    pnames[i] = NULL;
}

/* Return the portal buffer given a portal name. */
PortalBuffer *
PQparray (pname)
	char *pname;
{
    int i;
    if ((i = get_portal_index (pname)) == -1) 
	return ((PortalBuffer *) NULL);
    return (portals[i]->portal);
}

/* Return 1 if an asynchronized portal. */
int
PQrulep (portal)
	PortalBuffer *portal;
{
    return (portal->rule_p);
}

/* Return the number of tuples in a portal buffer. */
int
PQntuples (portal)
	PortalBuffer *portal;
{
    return (portal->no_tuples);
}

/* Return the number of tuple groups in a portal buffer. */
int PQngroups (portal)
	PortalBuffer *portal;
{
    return (portal->no_groups);
}

/* Return the group given the group_index. */
GroupBuffer *
findGroup (portal, group_index)
	PortalBuffer *portal;
	int group_index;
{
    GroupBuffer *group;

    group = portal->groups;
    while (group_index > 0 && group != NULL) {
	group = group->next;
	group_index--;
    }

    if (group == NULL)
	libpq_raise(PortalError, 
		    form("Group index %d out of bound.", group_index));
    return (group);
}

/* Return the number of tuples in a tuple group. */
PQntuplesGroup (portal, group_index)
	PortalBuffer *portal;
	int group_index;
{
    return (findGroup (portal, group_index)->no_tuples);
}

/* Return the number of fields in a tuple group. */
PQnfieldsGroup (portal, group_index)
	PortalBuffer *portal;
	int group_index;
{
    return (findGroup (portal, group_index)->no_fields);
}

/* Return the field index of a given field within a group. */
findFnumber (group, field_name)
       	GroupBuffer *group;
	char *field_name;
{
	TypeBlock *types;
	int i;

	types = group->types;

	for (i = 0; i < group->no_fields; i++, types++) 
	    if (strcmp (types->name, field_name) == 0)
		return (i);
	
	libpq_raise(PortalError, 
		    form("Field-name %s does not exist.", field_name));
}

/* If the field number if out of bound, signal an error. */
void
check_field_number (group, field_number)
	GroupBuffer *group;
	int field_number;
{
    if (field_number < 0 || field_number >= group->no_fields)
	libpq_raise(PortalError, 
		    form("Field number %d out of bound.", field_number));
}

/* Find the field name given the field index. */
char *
findFname (group, field_number)
	GroupBuffer *group;
	int field_number;
{
     check_field_number (group, field_number);
     return ((group->types + field_number)->name);
}

/* Return the field number (index) given the group index and the field name. */
int
PQfnumberGroup (portal, group_index, field_name)
	PortalBuffer *portal;
	int group_index;
	char *field_name;
{
    return (findFnumber (findGroup (portal, group_index), field_name));
}

/* Return the field (attribute) name given the group index and field index. */
char *
PQfnameGroup (portal, group_index, field_number)
	PortalBuffer *portal;
	int group_index;
	int field_number;
{
    return (findFname (findGroup (portal, group_index), field_number));
}

/* Return the tuple group that a particular tuple is in. */
GroupBuffer *
PQgroup (portal, tuple_index)
	PortalBuffer *portal;
	int tuple_index;
{
    GroupBuffer *group;

    if (tuple_index < 0 || tuple_index >= portal->no_tuples)
	libpq_raise(PortalError, 
		     form("tuple index %d out of bound.", tuple_index));
    group = portal->groups;

    while (tuple_index >= group->no_tuples) {
	tuple_index -= group->no_tuples;
	group = group->next;
    }

    return (group);
}

/* Return the index of the group that a partibular tuple is in. */
int
PQgetgroup (portal, tuple_index)
	PortalBuffer *portal;
	int tuple_index;
{
    GroupBuffer *group;
    int n = 0;

    if (tuple_index < 0 || tuple_index >= portal->no_tuples)
	libpq_raise(PortalError, 
		     form("tuple index %d out of bound.", tuple_index));
    group = portal->groups;

    while (tuple_index >= group->no_tuples) {
	n++;
	tuple_index -= group->no_tuples;
	group = group->next;
    }

    return (n);
}

/* Return the number of fields in a tuple. */
int
PQnfields (portal, tuple_index)
	PortalBuffer *portal;
	int tuple_index;
{
    return ((PQgroup (portal, tuple_index))->no_fields);
}

/* Return the field index of a given field name within a tuple. */
int
PQfnumber (portal, tuple_index, field_name)
	PortalBuffer *portal;
	int tuple_index;
	char *field_name;
{
    return (findFnumber (PQgroup (portal, tuple_index), field_name));
}

/* Return the name of a field. */
char *
PQfname (portal, tuple_index, field_number)
	PortalBuffer *portal;
	int tuple_index;
	int field_number;
{
    return (findFname (PQgroup (portal, tuple_index), field_number));
}

/* Return the type of a field. */
int 
PQftype (portal, tuple_index, field_number)
	PortalBuffer *portal;
	int tuple_index;
	int field_number;
{
    GroupBuffer *group;
    
    group = PQgroup (portal, tuple_index);
    check_field_number (group, field_number);
    
    return ((group->types + field_number)->adtid);
}

/* Return 1 if the two tuples have the same type (in the same group). */
int
PQsametype (portal, tuple_index1, tuple_index2)
	PortalBuffer *portal;
	int tuple_index1, tuple_index2;
{
    if (PQgroup (portal, tuple_index1) == PQgroup (portal, tuple_index2))
	return (1);
    return (0);
}

/* Return an attribute (field) value. */
char *
PQgetvalue (portal, tuple_index, field_number)
	PortalBuffer *portal;
	int tuple_index;
	int field_number;
{
    GroupBuffer *group;
    TupleBlock  *tuple_ptr;

    if (tuple_index < 0 || tuple_index >= portal->no_tuples)
	libpq_raise(PortalError, 
		    form("tuple index %d out of bound.", tuple_index));
    group = portal->groups;


    while (tuple_index >= group->no_tuples) {
	tuple_index -= group->no_tuples;
	group = group->next;
    }
    tuple_ptr = group->tuples;    

    while (tuple_index >= TupleBlockSize) {
	tuple_index -= TupleBlockSize;
	tuple_ptr = tuple_ptr->next;
    }

    check_field_number (group, field_number);
    
    return (tuple_ptr->values[tuple_index][field_number]);
}

