/* ----------------------------------------------------------------
 *   FILE
 *	be-dumpdata.c
 *	
 *   DESCRIPTION
 *	support for collection of returned tuples from an internal
 *	PQ call into a backend buffer.
 *
 *   INTERFACE ROUTINES
 *	be_portalinit    - initialize backend portal administration
 *	be_portalpush    - add a portal to the top of the portal stack
 *	be_portalpop     - remove portal on the top of the stack & return it
 *	be_currentportal - return the top portal on the portal stack
 *	be_newportal     - return a new portal.
 *	be_portalinit    - initialize backend portal expected to hold results.
 *	be_printtup      - add a tuple to a backend portal
 *
 *   NOTES
 *	Since backend user-defined operators can call queries
 *	which in turn call user-defined operators can call queries...
 *	we have to keep track of portals on a stack.  BeginCommand()
 *	puts portals on the stack and the PQ functions remove them.
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#include "tmp/postgres.h"

RcsId("$Header$");

#include "tmp/libpq-be.h"

#include "access/heapam.h"
#include "access/htup.h"
#include "storage/buf.h"
#include "tmp/simplelists.h"
#include "utils/memutils.h"
#include "utils/fmgr.h"
#include "utils/log.h"
#include "utils/exc.h"

#include "catalog/syscache.h"
#include "catalog/pg_type.h"

extern ObjectId typtoout();

/* ----------------
 *	backend portal stack for recursive PQexec calls
 * ----------------
 */
typedef struct PortalStackElement {
    PortalEntry *entry;
    SLNode 	 Node;
} PortalStackElement;

SLList be_portalstack;

/* ----------------
 *	be_portalinit - initialize backend portal administration
 *
 *	This is called once from InitPostgres() to initialize
 *	the portal stack.
 * ----------------
 */
void
be_portalinit()
{
    SLNewList(&be_portalstack, offsetof(PortalStackElement, Node));
}

/* ----------------
 *	be_portalpush - add a portal to the top of the portal stack
 *
 *	used by BeginCommand()
 * ----------------
 */
void
be_portalpush(entry)
    PortalEntry *entry;
{
    PortalStackElement *elemptr;

    elemptr = (PortalStackElement *) palloc(sizeof(PortalStackElement));
    elemptr->entry = entry;
    SLNewNode(&(elemptr->Node));
    SLAddTail(&be_portalstack, &(elemptr->Node));
}

/* ----------------
 *	be_portalpop - remove the portal on the top of the stack & return it
 *
 *	used by PQexec()
 * ----------------
 */
PortalEntry *
be_portalpop()
{
    PortalStackElement *elemptr;
    PortalEntry *entry;
    
    elemptr = (PortalStackElement *) SLRemTail(&be_portalstack);
    if (elemptr == NULL)
	return NULL;
	
    entry = elemptr->entry;
    pfree(elemptr);
    return entry;
}

/* ----------------
 *	be_currentportal - return the top portal on the portal stack
 *
 *	used by be_printtup()
 * ----------------
 */
PortalEntry *
be_currentportal()
{
    PortalStackElement *elemptr;
    
    elemptr = (PortalStackElement *) SLGetTail(&be_portalstack);
    if (elemptr == NULL)
	return NULL;
	
    return elemptr->entry;
}

/* ----------------
 *	be_newportal - return a new portal.
 *
 *	If the user-defined function does not specify a portal name,
 *	we generate a unique one.  Names are generated from a combination
 *	of a postgres oid and an integer counter which is incremented
 *	every time we ask for a local portal.
 *
 *	used by BeginCommand()
 * ----------------
 */

ObjectId be_portaloid;
u_int	 be_portalcnt = 0;

PortalEntry *
be_newportal(pname)   
    String pname;
{
    PortalEntry *entry;

    if (pname == NULL) {
	char buf[32];
	
	if (be_portalcnt == 0)
	    be_portaloid = newoid();
	be_portalcnt++;
	
	sprintf(buf, "be_%d_%d", be_portaloid, be_portalcnt);
	pname = palloc(strlen(buf));
	strcpy(pname, buf);
    }
    
    entry = pbuf_setup(pname);
}

/* ----------------
 *	be_typeinit - initialize backend portal expected to hold
 *			query results.
 *
 *	used by BeginCommand()
 * ----------------
 */
void
be_typeinit(entry, attrs, natts)
    PortalEntry 	*entry;
    struct attribute 	**attrs;
    int 		natts;
{
    PortalBuffer 	*portal;
    GroupBuffer 	*group;
    TypeBlock 		*types, *type;
    int 	i;

    /* ----------------
     *	add a new portal group to the portal
     * ----------------
     */
    portal = entry->portal;
    portal->no_groups++;
    portal->groups = group = pbuf_addGroup(portal);
    group->no_fields = natts;

    /* ----------------
     *	initialize portal group type info
     * ----------------
     */
    if (natts > 0) {
	type = types = group->types = pbuf_addTypes(natts);
	for (i = 0; i < natts; ++i) {
	    strncpy(types->name, attrs[i]->attname, NameLength);
	    type->adtid = attrs[i]->atttypid;
	    type->adtsize = attrs[i]->attlen;
	    type++;
	}
    }
}
	    
/* ----------------
 *	be_printtup - add a tuple to a backend portal
 *
 *	used indirectly by ExecRetrieve()
 *
 *	This code is pretty much copied from printtup(), dump_type()
 *	and dump_data().  -cim 2/12/91
 * ----------------
 */
void
be_printtup( tuple, typeinfo) 
    HeapTuple		tuple;
    struct attribute 	*typeinfo[];
{
    int		i;
    char	*attr;
    Boolean	isnull;
    ObjectId	typoutput;
    
    PortalEntry  *entry = NULL;
    PortalBuffer *portal = NULL;
    GroupBuffer  *group = NULL ;
    TupleBlock 	 *tuples = NULL;
    char 	 **values;
    
    /* ----------------
     *	for debugging 
     * ----------------
     */
    debugtup(tuple, typeinfo);
    
    /* ----------------
     *  get the current portal and group
     * ----------------
     */
    entry = be_currentportal();
    portal = entry->portal;
    group = portal->groups;
    
    /* ----------------
     *	If no tuple block yet, allocate one.
     *  If the current block is full, allocate another one.
     * ----------------
     */
    if (group->tuples == NULL) {
	tuples = group->tuples = pbuf_addTuples();
	tuples->tuple_index = 0;
    } else if (tuples->tuple_index == TupleBlockSize) {
	tuples->next = pbuf_addTuples();
	tuples = tuples->next;
	tuples->tuple_index = 0;
    }

    /* ----------------
     *	Allocate space for a tuple.
     * ----------------
     */
    tuples->values[tuples->tuple_index] = pbuf_addTuple(tuple->t_natts);
    
    /* ----------------
     *	copy printable representations of the tuple's attributes
     *  to the portal.
     *
     *  This seems silly, because the user's function which is calling
     *  PQexec() or PQfn() will probably just convert this back into the
     *  internal form anyways, but the point here is to provide a uniform
     *  libpq interface and this is how the fe libpq interface currently
     *  works.  Pretty soon we'll have to add code to let the fe or be
     *  select the desired data representation and then deal with that.
     *  This should not be too hard, as there already exist typrecieve()
     *  and typsend() procedures for user-defined types (see pg_type.h)
     *  -cim 2/11/91
     * ----------------
     */
    values = tuples->values[tuples->tuple_index];
    for (i = 0; i < tuple->t_natts; i++) {
	attr = heap_getattr(tuple, InvalidBuffer, i+1, typeinfo, &isnull);
	typoutput = typtoout((ObjectId) typeinfo[i]->atttypid);

	if (!isnull && ObjectIdIsValid(typoutput)) {
	    values[i] = fmgr(typoutput, attr);
	} else 
	    values[i] = NULL;
    }
    
    /* ----------------
     *	increment tuple group counters
     * ----------------
     */
    group->no_tuples++;
    tuples->tuple_index++;
}
