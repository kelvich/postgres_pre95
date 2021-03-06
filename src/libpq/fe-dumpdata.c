/* ----------------------------------------------------------------
 *   FILE
 *	fe-dumpdata.c
 *	
 *   DESCRIPTION
 *	Dump the returned tuples into a frontend buffer
 *
 *   INTERFACE ROUTINES
 *	dump_data 	- Read and process the data stream from backend
 *
 *   NOTES
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#include "tmp/c.h"

#include "tmp/simplelists.h"
#include "tmp/libpq-fe.h"
#include "utils/exc.h"

RcsId("$Header$");

/* Define constants. */

#define BYTELEN 8
#define MAXBYTES 128	/* MAXFIELDS / BYTELEN */
#define MAXFIELDS 512

/* --------------------------------
 *	dump_type - Dump the attributes
 * --------------------------------
 */
static
void
dump_type(types, nfields)
    TypeBlock *types;
    int nfields;
{
    int i;
    TypeBlock *type;

    type = types;

    for (i = 0; i < nfields; i++) {
	pq_getstr(type->name, NameLength);
	type->adtid = pq_getint(4);
	type->adtsize = pq_getint(2);
	type++;
    }
}

/* --------------------------------
 *	dump_tuple - Dump a tuple
 * --------------------------------
 */
static
void
dump_tuple(values,lengths, nfields)
    char **values;
    int *lengths;
    int nfields;
{
    char 	bitmap[MAXFIELDS];
    int 	bitmap_index = 0;
    int 	i;
    unsigned 	nbytes;		/* the number of bytes in bitmap */
    char 	bmap;		/* One byte of the bitmap */
    int 	bitcnt = 0; 	/* number of bits examined in current byte */
    int 	vlen;		/* length of the current field value */

    nbytes = nfields / BYTELEN;
    if ((nfields % BYTELEN) > 0) 
	nbytes++;

    pq_getnchar(bitmap, 0, nbytes);
    bmap = bitmap[bitmap_index];

    /* Read in all the attribute values. */
    for (i = 0; i < nfields; i++) {
	/* If the field value is absent, do nothing. */
	if (!(bmap & 0200))
	    values[i] = NULL;
	else {
	    /* Get the value length (the first four bytes are for length). */
	    vlen = pq_getint(4) - 4;
	    /* Allocate storage for the value. */
	    values[i] = pbuf_addValues(vlen + 1);
	    /* Read in the value. */
	    pq_getnchar(values[i], 0, vlen);
	    lengths[i] = vlen;
	    /* Put an end of string there to make life easier. */
	    values[i][vlen] = '\0';
	    pqdebug("%s", values[i]);
	}

	/* Get the approriate bitmap. */
	bitcnt++;
	if (bitcnt == BYTELEN) {
	    bitmap_index++;
	    bmap = bitmap[bitmap_index];
	    bitcnt = 0;
	} else 
	    bmap <<= 1;
    }
}
/* --------------------------------
 *	dump_tuple_internal - Dump a tuple in internal format
 * --------------------------------
 */
static
void
dump_tuple_internal(values, lengths, nfields)
    char **values;
    int *lengths;
    int nfields;
{
    char 	bitmap[MAXFIELDS];
    int 	bitmap_index = 0;
    int 	i;
    unsigned 	nbytes;		/* the number of bytes in bitmap */
    char 	bmap;		/* One byte of the bitmap */
    int 	bitcnt = 0; 	/* number of bits examined in current byte */
    int 	vlen;		/* length of the current field value */

    nbytes = nfields / BYTELEN;
    if ((nfields % BYTELEN) > 0) 
	nbytes++;

    pq_getnchar(bitmap, 0, nbytes);
    bmap = bitmap[bitmap_index];

    /* Read in all the attribute values. */
    for (i = 0; i < nfields; i++) {
	/* If the field value is absent, do nothing. */
	if (!(bmap & 0200))
	    values[i] = NULL;
	else {
	    /* For each attribute, we get:
	       Length (4 bytes),
	       Data (n bytes)
	       */

	    vlen = pq_getint(4);
	    /* Allocate storage for the value. */
	    values[i] = pbuf_addValues(vlen + 1);
	    /* Read in the value. */
	    pq_getnchar(values[i], 0, vlen);
	    lengths[i] = vlen;
	    /* Put an end of string there to make life easier. */
	    values[i][vlen] = '\0';
	    pqdebug("%s", values[i]);
	}

	/* Get the approriate bitmap. */
	bitcnt++;
	if (bitcnt == BYTELEN) {
	    bitmap_index++;
	    bmap = bitmap[bitmap_index];
	    bitcnt = 0;
	} else 
	    bmap <<= 1;
    }
}

/* --------------------------------
 *	finish_dump - End of a command (data stream)
 * --------------------------------
 */
static
void
finish_dump()
{
    char command[command_length];
    int temp;

    temp = pq_getint(4);
    pq_getstr(command, command_length);

    pqdebug("return code is %d",(char *)temp);
    pqdebug("command is %s",command);
}

/* --------------------------------
 *	dump_data - Read and process the data stream from backend
 * --------------------------------
 */
int 
dump_data(portal_name, rule_p)
    char *portal_name;
    int rule_p;
{
    char 	 id[2];
    char 	 pname[portal_name_length];
    PortalEntry  *entry = NULL;
    PortalBuffer *portal = NULL;
    GroupBuffer  *group = NULL;
    TypeBlock 	 *types = NULL;
    TupleBlock 	 *tuples = NULL;

    int ntuples = 0;	/* the number of tuples in current group */
    int nfields = 0;	/* the number of fields in current group */

    (void) strncpy(pname, portal_name, portal_name_length);

    /* If portal buffer is not allocated, do it now. */
    /* if ((portal = PQparray(pname)) == NULL) */
    entry = pbuf_setup(pname);
    portal = entry->portal;

    /* If an asynchronized portal, set the flag. */
    if (rule_p)
	portal->rule_p = 1;

    /* dump_data is called only when id[0] = 'T'. */
    id[0] = 'T';

    /* Process the data stream. */
    while (1) {
	switch (id[0]) {
	case 'T':
	    /* A new tuple group. */
	    /* If this is not the first group, record the number of 
	       tuples in the previous group. */
	    if (group != NULL) {
		group->no_tuples = ntuples;
		/* Add the number of tuples in last group to the total. */
		portal->no_tuples += ntuples;
	    }

	    /* Increment the number of tuple groups. */
	    portal->no_groups++;
	    group = pbuf_addGroup(portal);

	    /* Read in the number of fields (attributes) for this group. */
	    nfields = group->no_fields = pq_getint(2);
	    if (nfields > 0) {
	        types = group->types = pbuf_addTypes(nfields);
	        dump_type(types, nfields);
	    }
	    break;
	case 'B':
	    /* A tuple in internal (binary) format. */

	    /* If no tuple block yet, allocate one. */
	    /* If the current block is full, allocate another one. */
	    if (group->tuples == NULL) {
		tuples = group->tuples = pbuf_addTuples();
		tuples->tuple_index = 0;
	    } else if (tuples->tuple_index == TupleBlockSize) {
		tuples->next = pbuf_addTuples();
		tuples = tuples->next;
		tuples->tuple_index = 0;
	    }

	    /* Allocate space for a tuple. */
	    tuples->values[tuples->tuple_index] =pbuf_addTuple(nfields);
	    tuples->lengths[tuples->tuple_index] = pbuf_addTupleValueLengths(nfields);

	    /* Dump a tuple internal format. */
	    dump_tuple_internal(tuples->values[tuples->tuple_index],
				tuples->lengths[tuples->tuple_index],
				nfields);
	    ntuples++;
	    tuples->tuple_index++;
	    break;
	case 'D':
	    /* A tuple. */

	    /* If no tuple block yet, allocate one. */
	    /* If the current block is full, allocate another one. */
	    if (group->tuples == NULL) {
		tuples = group->tuples = pbuf_addTuples();
		tuples->tuple_index = 0;
	    } else if (tuples->tuple_index == TupleBlockSize) {
		tuples->next = pbuf_addTuples();
		tuples = tuples->next;
		tuples->tuple_index = 0;
	    }

	    /* Allocate space for a tuple. */
	    tuples->values[tuples->tuple_index] =pbuf_addTuple(nfields);
	    tuples->lengths[tuples->tuple_index] = pbuf_addTupleValueLengths(nfields);

	    /* Dump a tuple. */
	    dump_tuple(tuples->values[tuples->tuple_index],
		       tuples->lengths[tuples->tuple_index],
		       nfields);
	    ntuples++;
	    tuples->tuple_index++;
	    break;
	case 'A':
	    strcpy(PQerrormsg, 
		   "FATAL: dump_data: asynchronous portals not supported\n");
	    fputs(PQerrormsg, stderr);
	    pqdebug("%s", PQerrormsg);
	    PQreset();
	    return(-1);
#if 0
	    /* Tuples returned by alerters. */
	    /* Finish up with the current portal. */
	    group->no_tuples = ntuples;
	    portal->no_tuples += ntuples;

	    /* Process the asynchronized portal. */
	    /* This part of the protocol is not very clear. */
	    pq_getint(4);
	    pq_getstr(pname, portal_name_length);
	    pqdebug("Asynchronized portal: %s", pname);

	    entry = pbuf_setup(pname);
	    portal = entry->portal;
	    portal->rule_p = 1;
	    group = NULL;
	    ntuples = 0;
	    nfields = 0;
	    tuples = NULL;
	    break;
#endif
	case 'C':
	    /* Command, end of the data stream. */
	    /* Record the number of tuples in the last group. */
	    group->no_tuples = ntuples;
	    portal->no_tuples += ntuples;
	    finish_dump();
	    return(1);
	case 'E':
	    (void) pq_print_elog(NULL, false);
	    return(-1);
	case 'N':
	    if (pq_print_elog(NULL, false) < 0)
		return(-1);
	    break;
	default:
	    /*
	     * This should never happen, but frequently does (usually 
	     * when the backend dumps core).
	     */
	    sprintf(PQerrormsg, "FATAL: dump_data: Unexpected identifier: %c",
		    id[0]);
	    fputs(PQerrormsg, stderr);
	    pqdebug("%s", PQerrormsg);
	    PQreset();
	    return(-1);
	}

	if (pq_read_id(id, "dump_data") > 0)
	    return(-1);
	
    	read_remark(id);
	pqdebug("The identifier is: %c", (char *) id[0]);
    }
}
