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
 *	PQrulep 	- Return 1 if an asynchronous portal
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

int	PQtracep = 0;		/* 1 to print out debugging messages */
FILE    *debug_port = (FILE *) NULL;

static
in_range(msg, value, min, max)
    char *msg;
    int value, min, max;
{
    if (value < min || value >= max) {
	(void) sprintf(PQerrormsg, "FATAL: %s is not in range [%d,%d)\n",
		       msg, value, min, max);
	pqdebug("%s", PQerrormsg);
	fputs(PQerrormsg, stderr);
	return(0);
    }
    return(1);
}

static
valid_pointer(msg, ptr)
    char *msg;
    void *ptr;
{
    if (!ptr) {
	(void) sprintf(PQerrormsg, "FATAL: %s\n", msg);
	pqdebug("%s", PQerrormsg);
	fputs(PQerrormsg, stderr);
	return(0);
    }
    return(1);
}

/* ----------------------------------------------------------------
 *			PQ utility routines
 * ----------------------------------------------------------------
 */
void
pqdebug(target, msg)
    char *target, *msg;
{
    if (!target)
	return;

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
    if (!target)
	return;

    if (PQtracep) {
	/*
	 * if nothing else was suggested default to stdout
	 */
	if (!debug_port)
	    debug_port = stdout;
	fprintf(debug_port, target, msg1, msg2);
	fprintf(debug_port, "\n");
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
 * 	If rule_p, only return asynchronous portals. 
 * --------------------------------
 */
int
PQnportals(rule_p)
    int rule_p;
{
    int i, n = 0;
    
    for (i = 0; i < portals_array_size; ++i) {
	if (portals[i] && portals[i]->portal) {
	    if (!rule_p || portals[i]->portal->rule_p) {
		++n;
	    }
	}
    }
    return(n);
}

/* --------------------------------
 *	PQpnames - Return all the portal names
 * 	If rule_p, only return asynchronous portals. 
 *         the caller must have allocated sufficient memory for char** pnames
 * --------------------------------
 */
void
PQpnames(pnames, rule_p)
    char **pnames;
    int rule_p;
{
    int i;
    
    if (!valid_pointer("PQpnames: invalid name buffer", pnames))
	return;
    
    for (i = 0; i < portals_array_size; ++i) {
	if (portals[i] && portals[i]->portal) {
	    if (!rule_p || portals[i]->portal->rule_p) {
		(void) strncpy(pnames[i], portals[i]->name, PortalNameLength);
		continue;
	    }
	}
	pnames[i][0] = '\0';
    }
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

    if (!valid_pointer("PQparray: invalid name buffer", pname))
	return;
    
    if ((i = pbuf_getIndex(pname)) < 0)
	return((PortalBuffer *) NULL);
    return(portals[i]->portal);
}

/* --------------------------------
 *	PQrulep - Return 1 if an asynchronous portal
 * --------------------------------
 */
int
PQrulep(portal)
    PortalBuffer *portal;
{
    if (!valid_pointer("PQrulep: invalid portal pointer", portal))
	return(-1);

    return(portal->rule_p);
}

/* --------------------------------
 *	PQntuples - Return the number of tuples in a portal buffer
 * --------------------------------
 */
int
PQntuples(portal)
    PortalBuffer *portal;
{
    if (!valid_pointer("PQntuples: invalid portal pointer", portal))
	return(-1);

    return(portal->no_tuples);
}

int
PQninstances(portal)
    PortalBuffer *portal;
{
    return(PQntuples(portal));
}

/* --------------------------------
 *	PQngroups - Return the number of tuple groups in a portal buffer
 * --------------------------------
 */
int
PQngroups(portal)
    PortalBuffer *portal;
{
    if (!valid_pointer("PQngroups: invalid portal pointer", portal))
	return(-1);

    return(portal->no_groups);
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
    GroupBuffer *gbp;

    if (!valid_pointer("PQntuplesGroup: invalid portal pointer", portal) ||
	!in_range("PQntuplesGroup: group index",
		  group_index, 0, portal->no_groups))
	return(-1);
    
    if (gbp = pbuf_findGroup(portal, group_index))
	return(gbp->no_tuples);
    return(-1);
}

int
PQninstancesGroup(portal, group_index)
    PortalBuffer *portal;
    int 	 group_index;
{
    return(PQntuplesGroup(portal, group_index));
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
    GroupBuffer *gbp;

    if (!valid_pointer("PQnfieldsGroup: invalid portal pointer", portal) ||
	!in_range("PQnfieldsGroup: group index",
		  group_index, 0, portal->no_groups))
	return(-1);

    if (gbp = pbuf_findGroup(portal, group_index))
	return(gbp->no_fields);
    return(-1);
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
    GroupBuffer *gbp;

    if (!valid_pointer("PQfnumberGroup: invalid portal pointer", portal) ||
	!valid_pointer("PQfnumberGroup: invalid field name pointer",
		       field_name) ||
	!in_range("PQfnumberGroup: group index",
		  group_index, 0, portal->no_groups))
	return(-1);
    
    if (gbp = pbuf_findGroup(portal, group_index))
	return(pbuf_findFnumber(gbp, field_name));
    return(-1);
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
    GroupBuffer *gbp;

    if (!valid_pointer("PQfnameGroup: invalid portal pointer", portal) ||
	!in_range("PQfnameGroup: group index",
		  group_index, 0, portal->no_groups))
	return((char *) NULL);

    if ((gbp = pbuf_findGroup(portal, group_index)) &&
	in_range("PQfnameGroup: field number",
		 field_number, 0, gbp->no_fields))
	return(pbuf_findFname(gbp, field_number));
    return((char *) NULL);
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
    GroupBuffer *gbp;
    int tuple_count = 0;
    
    if (!valid_pointer("PQgroup: invalid portal pointer", portal) ||
	!in_range("PQgroup: tuple index",
		  tuple_index, 0, portal->no_tuples))
	return((GroupBuffer *) NULL);
    
    for (gbp = portal->groups;
	 gbp && tuple_index >= (tuple_count += gbp->no_tuples);
	 gbp = gbp->next)
	;
    if (!in_range("PQgroup: tuple not found: tuple index",
		  tuple_index, 0, tuple_count))
	return((GroupBuffer *) NULL);
    return(gbp);
}

/* --------------------------------
 *	PQgetgroup - Return the index of the group that a
 *		     particular tuple is in
 * --------------------------------
 */
int
PQgetgroup(portal, tuple_index)
    PortalBuffer *portal;
    int		 tuple_index;
{
    GroupBuffer *gbp;
    int tuple_count = 0, group_count = 0;

    if (!valid_pointer("PQgetgroup: invalid portal pointer", portal) ||
	!in_range("PQgetgroup: tuple index",
		  tuple_index, 0, portal->no_tuples))
	return(-1);
    
    for (gbp = portal->groups;
	 gbp && tuple_index >= (tuple_count += gbp->no_tuples);
	 gbp = gbp->next)
	++group_count;
    if (!gbp || !in_range("PQgetgroup: tuple not found: tuple index",
			  tuple_index, 0, tuple_count))
	return(-1);
    return(group_count);
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
    GroupBuffer *gbp;
    
    if (!valid_pointer("PQnfields: invalid portal pointer", portal) ||
	!in_range("PQnfields: tuple index",
		  tuple_index, 0, portal->no_tuples))
	return(-1);

    if (gbp = PQgroup(portal, tuple_index))
	return(gbp->no_fields);
    return(-1);
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
    GroupBuffer *gbp;

    if (!valid_pointer("PQfnumber: invalid portal pointer", portal) ||
	!valid_pointer("PQfnumber: invalid field name pointer", field_name) ||
	!in_range("PQfnumber: tuple index",
		  tuple_index, 0, portal->no_tuples))
	return(-1);

    if (gbp = PQgroup(portal, tuple_index))
	return(pbuf_findFnumber(gbp, field_name));
    return(-1);
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
    GroupBuffer *gbp;
        
    if (!valid_pointer("PQfname: invalid portal pointer", portal) ||
	!in_range("PQfname: tuple index",
		  tuple_index, 0, portal->no_tuples))
	return((char *) NULL);

    if ((gbp = PQgroup(portal, tuple_index)) &&
	in_range("PQfname: field number",
		 field_number, 0, gbp->no_fields))
	return(pbuf_findFname(gbp, field_number));
    return((char *) NULL);
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
    GroupBuffer *gbp;
    
    if (!valid_pointer("PQfname: invalid portal pointer", portal) ||
	!in_range("PQfname: tuple index",
		  tuple_index, 0, portal->no_tuples))
	return(-1);

    if ((gbp = PQgroup(portal, tuple_index)) &&
	in_range("PQfname: field number", field_number, 0, gbp->no_fields))
	return(gbp->types[field_number].adtid);
    return(-1);
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
    GroupBuffer *gbp1, *gbp2;
    
    if (!valid_pointer("PQsametype: invalid portal pointer", portal) ||
	!in_range("PQsametype: tuple index 1",
		  tuple_index1, 0, portal->no_tuples) ||
	!in_range("PQsametype: tuple index 2",
		  tuple_index2, 0, portal->no_tuples))
	return(-1);

    gbp1 = PQgroup(portal, tuple_index1);
    gbp2 = PQgroup(portal, tuple_index2);
    if (gbp1 && gbp2)
	return(gbp1 == gbp2);
    return(-1);
}

static
TupleBlock *
PQGetTupleBlock(portal, tuple_index, tuple_offset)
    PortalBuffer *portal;
    int tuple_index;
    int *tuple_offset;
{
    GroupBuffer *gbp;
    TupleBlock  *tbp;
    int tuple_count = 0;

    if (!valid_pointer("PQGetTupleBlock: invalid portal pointer", portal) ||
	!valid_pointer("PQGetTupleBlock: invalid offset pointer",
		       tuple_offset) ||
	!in_range("PQGetTupleBlock: tuple index",
		  tuple_index, 0, portal->no_tuples))
	return((TupleBlock *) NULL);
    
    for (gbp = portal->groups;
	 gbp && tuple_index >= (tuple_count += gbp->no_tuples);
	 gbp = gbp->next)
	;
    if (!gbp ||
	!in_range("PQGetTupleBlock: tuple not found: tuple index",
		  tuple_index, 0, tuple_count))
	return((TupleBlock *) NULL);
    tuple_count -= gbp->no_tuples;
    for (tbp = gbp->tuples;
	 tbp && tuple_index >= (tuple_count += TupleBlockSize);
	 tbp = tbp->next)
	;
    if (!tbp ||
	!in_range("PQGetTupleBlock: tuple not found: tuple index",
		  tuple_index, 0, tuple_count))
	return((TupleBlock *) NULL);
    tuple_count -= TupleBlockSize;
    
    *tuple_offset = tuple_index - tuple_count;
    return(tbp);
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
    TupleBlock *tbp;
    int tuple_offset;

    if (tbp = PQGetTupleBlock(portal, tuple_index, &tuple_offset))
	return(tbp->values[tuple_offset][field_number]);
    return((char *) NULL);
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
    TupleBlock *tbp;
    int tuple_offset;

    if (tbp = PQGetTupleBlock(portal, tuple_index, &tuple_offset))
	return(tbp->lengths[tuple_offset][field_number]);
    return(-1);
}

/* ----------------
 *	PQclear		- free storage claimed by named portal
 * ----------------
 */
void
PQclear(pname)
    char *pname;
{    
    if (!valid_pointer("PQclear: invalid portal name pointer", pname))
	return;
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
    strncpy(nPtr->relname, relname, NAMEDATALEN);
    nPtr->be_pid = pid;
    nPtr->valid = 1;
    SLNewNode(&nPtr->Node);
    SLAddTail(&pqNotifyList,&nPtr->Node);

}
