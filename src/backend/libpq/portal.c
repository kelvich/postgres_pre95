/* ----------------------------------------------------------------
 *   FILE
 *	portal.c
 *	
 *   DESCRIPTION
 *	generalized portal support routines
 *
 *   UTILITY ROUTINES
 *	pqdebug		- send a string to the debugging output port
 *	pqdebug2	- send two strings to stdout
 *	PQtrace		- turn on pqdebug() tracing
 *	PQuntrace	- turn off pqdebug() tracing
 *
 *   INTERFACE ROUTINES
 *	PQnportals 	- Return the number of open portals. 
 *	PQpnames 	- Return all the portal names
 *	PQparray 	- Return the portal buffer given a portal name
 *	PQrulep 	- Return 1 if an asynchronized portal
 *	PQntuples 	- Return the number of tuples in a portal buffer
 *	PQninstances	-   same as PQntuples using object terminology
 *	PQngroups 	- Return the number of tuple groups in a portal buffer
 *	PQntuplesGroup 	- Return the number of tuples in a tuple group
 *	PQninstancesGroup  - same as PQntuplesGroup using object terminology
 *	PQnfieldsGroup 	- Return the number of fields in a tuple group
 *	PQfnumberGroup 	- Return field number given (group index, field name)
 *	PQfnameGroup 	- Return field name given (group index, field index)
 *	PQgroup 	- Return the tuple group that a particular tuple is in
 *	PQgetgroup 	- Return the index of the group that a tuple is in
 *	PQnfields 	- Return the number of fields in a tuple
 *	PQfnumber 	- Return the field index of a field name in a tuple
 *	PQfname 	- Return the name of a field
 *	PQftype 	- Return the type of a field
 *	PQsametype 	- Return 1 if the two tuples have the same type
 *	PQgetvalue 	- Return an attribute (field) value
 *	PQgetlength 	- Return an attribute (field) length
 *	PQclear		- free storage claimed by named portal
 *      PQnotifies      - Return a list of relations on which notification 
 *                        has occurred.
 *      PQremoveNotify  - Remove this notification from the list.
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

#include <string.h>

#include "tmp/c.h"
#include "tmp/simplelists.h"
#include "tmp/libpq.h"
#include "utils/exc.h"

RcsId("$Header$");

/* ----------------
 *	exceptions
 * ----------------
 */
Exception MemoryError = {"Memory Allocation Error"};
Exception PortalError = {"Invalid arguments to portal functions"};
Exception PostquelError = {"Postquel Error"};
Exception ProtocolError = {"Protocol Error"};
char PQerrormsg[error_msg_length];

int	PQtracep = 0;		/* 1 to print out debugging message */
FILE    *debug_port = (FILE *)NULL;

/* ----------------------------------------------------------------
 *			PQ utility routines
 * ----------------------------------------------------------------
 */
void
pqdebug (target, msg)
char *target, *msg;
{
    if (PQtracep) {
	/*
	 * if nothing else was suggested default to stdout
	 */
	if (!debug_port)
	    debug_port = stdout;
	fprintf(debug_port, target, msg);
	fprintf(debug_port, "\n");
    }
}

void
pqdebug2(target, msg1, msg2)
char *target, *msg1, *msg2;
{
    if (PQtracep) {
	printf(target, msg1, msg2);
	printf("\n");
    }
}

/* --------------------------------
 *	PQtrace() / PQuntrace()
 * --------------------------------
 */
void
PQtrace()
{
    PQtracep = 1;
}

void
PQuntrace()
{
    PQtracep = 0;
}

/* ----------------------------------------------------------------
 *		    PQ portal interface routines
 * ----------------------------------------------------------------
 */

/* --------------------------------
 *	PQnportals - Return the number of open portals. 
 * 	If rule_p, only return asynchronized portals. 
 * --------------------------------
 */
int
PQnportals(rule_p)
    int rule_p;
{
    int i, n = 0;
    
    for (i = 0; i < MAXPORTALS; i++) 
	if (portals[i] != NULL)
	    if (!rule_p || portals[i]->portal->rule_p)
		n++;
    
    return n;
}

/* --------------------------------
 *	PQpnames - Return all the portal names
 * 	If rule_p, only return asynchronized portals. 
 * --------------------------------
 */
void
PQpnames(pnames, rule_p)
     char *pnames[MAXPORTALS];
     int rule_p;
{
    int i;

    for (i = 0; i < MAXPORTALS; i++)
	if (portals[i] != NULL) {
	    if (!rule_p || portals[i]->portal->rule_p) 
		strcpy(pnames[i], portals[i]->name);
	    else pnames[i] = NULL;
	}
	else 
	    pnames[i] = NULL;
}

/* --------------------------------
 *	PQparray - Return the portal buffer given a portal name
 * --------------------------------
 */
PortalBuffer *
PQparray(pname)
    char *pname;
{
    int i;
    
    if ((i = pbuf_getIndex(pname)) == -1) 
	return (PortalBuffer *) NULL;
    
    return (portals[i]->portal);
}

/* --------------------------------
 *	PQrulep - Return 1 if an asynchronized portal
 * --------------------------------
 */
int
PQrulep(portal)
    PortalBuffer *portal;
{
    return (portal->rule_p);
}

/* --------------------------------
 *	PQntuples - Return the number of tuples in a portal buffer
 * --------------------------------
 */
int
PQntuples(portal)
    PortalBuffer *portal;
{
    return (portal->no_tuples);
}

int
PQninstances(portal)
    PortalBuffer *portal;
{
    return PQntuples(portal);
}

/* --------------------------------
 *	PQngroups - Return the number of tuple groups in a portal buffer
 * --------------------------------
 */
int
PQngroups(portal)
    PortalBuffer *portal;
{
    return (portal->no_groups);
}


/* --------------------------------
 *	PQntuplesGroup - Return the number of tuples in a tuple group
 * --------------------------------
 */
int
PQntuplesGroup(portal, group_index)
    PortalBuffer *portal;
    int 	 group_index;
{
    return
	pbuf_findGroup(portal, group_index)->no_tuples;
}

int
PQninstancesGroup(portal, group_index)
    PortalBuffer *portal;
    int 	 group_index;
{
    return
	PQntuplesGroup(portal, group_index);
}

/* --------------------------------
 *	PQnfieldsGroup - Return the number of fields in a tuple group
 * --------------------------------
 */
int
PQnfieldsGroup(portal, group_index)
    PortalBuffer *portal;
    int		 group_index;
{
    return
	pbuf_findGroup(portal, group_index)->no_fields;
}

/* --------------------------------
 *	PQfnumberGroup - Return the field number (index) given
 *			 the group index and the field name
 * --------------------------------
 */
int
PQfnumberGroup(portal, group_index, field_name)
    PortalBuffer *portal;
    int		 group_index;
    char	 *field_name;
{
    return
	pbuf_findFnumber( pbuf_findGroup(portal, group_index), field_name);
}

/* --------------------------------
 *	PQfnameGroup - Return the field (attribute) name given
 *			the group index and field index. 
 * --------------------------------
 */
char *
PQfnameGroup(portal, group_index, field_number)
    PortalBuffer *portal;
    int group_index;
    int field_number;
{
    return
	pbuf_findFname( pbuf_findGroup(portal, group_index), field_number);
}

/* --------------------------------
 *	PQgroup - Return the tuple group that a particular tuple is in
 * --------------------------------
 */
GroupBuffer *
PQgroup(portal, tuple_index)
    PortalBuffer *portal;
    int		 tuple_index;
{
    GroupBuffer *group;

    if (tuple_index < 0 || tuple_index >= portal->no_tuples)
	libpq_raise(&PortalError, 
		     form((int)"tuple index %d out of bound.", tuple_index));
    
    group = portal->groups;

    while (tuple_index >= group->no_tuples) {
	tuple_index -= group->no_tuples;
	group = group->next;
    }

    return (group);
}

/* --------------------------------
 *	PQgetgroup - Return the index of the group that a
 *		     partibular tuple is in
 * --------------------------------
 */
int
PQgetgroup(portal, tuple_index)
    PortalBuffer *portal;
    int		 tuple_index;
{
    GroupBuffer *group;
    int n = 0;

    if (tuple_index < 0 || tuple_index >= portal->no_tuples)
	libpq_raise(&PortalError, 
		     form((int)"tuple index %d out of bound.", tuple_index));
    
    group = portal->groups;

    while (tuple_index >= group->no_tuples) {
	n++;
	tuple_index -= group->no_tuples;
	group = group->next;
    }

    return (n);
}

/* --------------------------------
 *	PQnfields - Return the number of fields in a tuple
 * --------------------------------
 */
int
PQnfields(portal, tuple_index)
    PortalBuffer *portal;
    int		 tuple_index;
{
    return
	PQgroup(portal, tuple_index)->no_fields;
}

/* --------------------------------
 *	PQfnumber - Return the field index of a given
 *		    field name within a tuple. 
 * --------------------------------
 */
int
PQfnumber(portal, tuple_index, field_name)
    PortalBuffer *portal;
    int		 tuple_index;
    char	 *field_name;
{
    return
	pbuf_findFnumber( PQgroup(portal, tuple_index), field_name);
}

/* --------------------------------
 *	PQfname - Return the name of a field
 * --------------------------------
 */
char *
PQfname(portal, tuple_index, field_number)
    PortalBuffer *portal;
    int		 tuple_index;
    int		 field_number;
{
    return
	pbuf_findFname( PQgroup(portal, tuple_index), field_number);
}

/* --------------------------------
 *	PQftype - Return the type of a field
 * --------------------------------
 */
int 
PQftype(portal, tuple_index, field_number)
    PortalBuffer *portal;
    int		 tuple_index;
    int		 field_number;
{
    GroupBuffer *group;
    
    group = PQgroup(portal, tuple_index);
    pbuf_checkFnumber(group, field_number);
    
    return
	(group->types + field_number)->adtid;
}

/* --------------------------------
 *	PQsametype - Return 1 if the two tuples have the same type
 *			(in the same group)
 * --------------------------------
 */
int
PQsametype(portal, tuple_index1, tuple_index2)
    PortalBuffer *portal;
    int		 tuple_index1;
    int		 tuple_index2;
{
    return
	(PQgroup(portal, tuple_index1) == PQgroup(portal, tuple_index2));
}

/* --------------------------------
 *	PQgetvalue - Return an attribute (field) value
 * --------------------------------
 */
char *
PQgetvalue(portal, tuple_index, field_number)
    PortalBuffer *portal;
    int		 tuple_index;
    int		 field_number;
{
    GroupBuffer *group;
    TupleBlock  *tuple_ptr;

    if (tuple_index < 0 || tuple_index >= portal->no_tuples)
	libpq_raise(&PortalError, 
		    form((int)"tuple index %d out of bound.", tuple_index));
    
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

    pbuf_checkFnumber(group, field_number);
    
    return
	tuple_ptr->values[tuple_index][field_number];
}

/* --------------------------------
 *	PQgetlength - Return an attribute (field) length
 * --------------------------------
 */
int
PQgetlength(portal, tuple_index, field_number)
    PortalBuffer *portal;
    int		 tuple_index;
    int		 field_number;
{
    GroupBuffer *group;
    TupleBlock  *tuple_ptr;

    if (tuple_index < 0 || tuple_index >= portal->no_tuples)
	libpq_raise(&PortalError, 
		    form((int)"tuple index %d out of bound.", tuple_index));
    
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

    pbuf_checkFnumber(group, field_number);
    
    return
	tuple_ptr->lengths[tuple_index][field_number];
}

/* ----------------
 *	PQclear		- free storage claimed by named portal
 * ----------------
 */
void
PQclear(pname)
    char *pname;
{    
    pbuf_close(pname);
}

/*
 * async notification.
 * This is going away with pending rewrite of comm. code...
 */

SLList pqNotifyList;
static int initialized = 0;	/* statics in this module initialized? */

/* remove invalid notifies before returning */
void
PQcleanNotify()
{
    PQNotifyList *last = NULL;
    PQNotifyList *nPtr;
    for (nPtr = (PQNotifyList *)SLGetHead(&pqNotifyList);
	 nPtr != NULL;
	 nPtr = (PQNotifyList *)SLGetSucc(&nPtr->Node)) {
	if (last != NULL && last->valid == 0) {
	    SLRemove(&last->Node);
	    pbuf_free((caddr_t)last);
	}
	last = nPtr;
    }
    if (last != NULL && last->valid == 0) {
	SLRemove(&last->Node);
	pbuf_free((caddr_t)last);
    }
}


void
PQnotifies_init() {
    PQNotifyList *nPtr;

    if (! initialized) {
	initialized = 1;
	SLNewList(&pqNotifyList,offsetof(PQNotifyList,Node));
    } else {			/* clean all notifies */
	for (nPtr = (PQNotifyList *)SLGetHead(&pqNotifyList) ;
	     nPtr != NULL;
	     nPtr = (PQNotifyList *)SLGetSucc(&nPtr->Node)) {
	    nPtr->valid = 0;
	}
	PQcleanNotify();
    }
}

PQNotifyList *
PQnotifies()
{
    PQcleanNotify();
    return (PQNotifyList *)SLGetHead(&pqNotifyList);
}

void
PQremoveNotify(nPtr)
     PQNotifyList *nPtr;
{
    nPtr->valid = 0;		/* remove later */
}

void 
PQappendNotify(relname,pid)
     char *relname;
     int pid;
{
    PQNotifyList *nPtr;
    if (! initialized) {
	initialized = 1;
	SLNewList(&pqNotifyList,offsetof(PQNotifyList,Node));
    }
    nPtr = (PQNotifyList *)pbuf_alloc(sizeof(PQNotifyList));
    strcpy(nPtr->relname,relname);
    nPtr->be_pid = pid;
    nPtr->valid = 1;
    SLNewNode(&nPtr->Node);
    SLAddTail(&pqNotifyList,&nPtr->Node);
}
