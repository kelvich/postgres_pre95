/*
 * "$Header$"
 */

#include <stdio.h>

#include "tmp/postgres.h"
#include "tmp/globals.h"
#include "tmp/align.h"
#include "catalog/syscache.h"
#include "catalog/pg_type.h"
#include "catalog/pg_index.h"

#include "access/heapam.h"
#include "access/htup.h"
#include "access/itup.h"
#include "access/relscan.h"
#include "utils/rel.h"
#include "utils/log.h"
#include "tmp/daemon.h"
#include "utils/fmgr.h"
#include "machine.h"

/*
 * New copy code.
 * 
 * This code "knows" the following about tuples:
 * 
 */

static bool reading_from_input = false;

extern FILE *Pfout, *Pfin;

void
DoCopy(relname, binary, from, pipe, filename)

char *relname;
bool binary, from, pipe;
char *filename;

{
    FILE *fp;
	Relation rel, heap_openr();

    reading_from_input = pipe;

	rel = heap_openr(relname);
	if (rel == NULL) elog(WARN, "Copy: class %s does not exist.", relname);

    if (from)
    {
        if (pipe && IsUnderPostmaster) ReceiveCopyBegin();
        if (IsUnderPostmaster)
        {
            fp = pipe ? Pfin : fopen(filename, "r");
        }
        else
        {
            fp = pipe ? stdin : fopen(filename, "r");
        }
        if (fp == NULL) 
        {
            elog(WARN, "COPY: file %s could not be open for reading", filename);
        }
        CopyFrom(rel, binary, fp);
    }
    else
    {
        if (pipe && IsUnderPostmaster) SendCopyBegin();
        if (IsUnderPostmaster)
        {
            fp = pipe ? Pfout : fopen(filename, "w");
        }
        else
        {
            fp = pipe ? stdout : fopen(filename, "w");
        }
        if (fp == NULL) 
        {
            elog(WARN, "COPY: file %s could not be open for writing", filename);
        }
        CopyTo(rel, binary, fp);
    }
    if (!pipe)
    {
        fclose(fp);
    }
    else if (!from && !binary)
    {
        fputs(".\n", fp);
        if (IsUnderPostmaster) fflush(Pfout);
    }
}

CopyTo(rel, binary, fp)

Relation rel;
bool binary;
FILE *fp;

{
    HeapTuple tuple, heap_getnext();
    Relation heap_openr();
    HeapScanDesc scandesc, heap_beginscan();

    int32 attr_count, i;
    Attribute *attr;
    func_ptr *out_functions;
    int dummy;
    ObjectId out_func_oid;
    ObjectId *elements;
    Datum value;
    Boolean isnull = (Boolean) true;
    char *nulls;
    char *string;
    bool *byval;
    int32 ntuples;

    scandesc = heap_beginscan(rel, 0, NULL, NULL, NULL);

    attr_count = rel->rd_rel->relnatts;
    attr = (Attribute *) &rel->rd_att;

    if (!binary)
    {
        out_functions = (func_ptr *)
                         palloc(attr_count * sizeof(func_ptr));
        elements = (ObjectId *) palloc(attr_count * sizeof(ObjectId));
        for (i = 0; i < attr_count; i++) {
            out_func_oid = (ObjectId) GetOutputFunction(attr[i]->atttypid);
            fmgr_info(out_func_oid, &out_functions[i], &dummy);
            elements[i] = GetTypeElement(attr[i]->atttypid);
        }
    }
    else
    {
        nulls = (char *) palloc(attr_count);
        for (i = 0; i < attr_count; i++) nulls[i] = ' ';

        /* XXX expensive */

        ntuples = CountTuples(rel);
        fwrite(&ntuples, sizeof(int32), 1, fp);
    }

    for (tuple = heap_getnext(scandesc, NULL, NULL);
         tuple != NULL; 
         tuple = heap_getnext(scandesc, NULL, NULL))
    {
        for (i = 0; i < attr_count; i++)
        {
            value = (Datum) 
                    heap_getattr(tuple, InvalidBuffer, i+1, attr, &isnull);
            if (!binary)
            {
                if (!isnull)
                {
                    string = (char *) (out_functions[i]) (value, elements[i]);
                    CopyAttributeOut(fp, string);
                    pfree(string);
                }

                if (i == attr_count - 1)
                {
                    fputc('\n', fp);
                }
                else
                {
                    fputc('\t', fp);
                }
            }
            else
            {

            /*
             * only interesting thing heap_getattr tells us in this case
             * is if we have a null attribute or not.
             */

                if (isnull) nulls[i] = 'n';
            }
        }

        if (binary)
        {
            int32 null_ct = 0, length;

            for (i = 0; i < attr_count; i++) 
            {
                if (nulls[i] == 'n') null_ct++;
            }

            length = tuple->t_len - tuple->t_hoff;
            fwrite(&length, sizeof(int32), 1, fp);
            fwrite(&null_ct, sizeof(int32), 1, fp);
            if (null_ct > 0)
            {
                for (i = 0; i < attr_count; i++)
                {
                    if (nulls[i] == 'n')
                    {
                        fwrite(&i, sizeof(int32), 1, fp);
                        nulls[i] = ' ';
                    }
                }
            }
            fwrite((char *) tuple + tuple->t_hoff, length, 1, fp);
        }
    }

    heap_endscan(scandesc);
    if (binary)
    {
        pfree(nulls);
    }
    else
    {
        pfree(out_functions);
        pfree(elements);
    }

    heap_close(rel);
}

CopyFrom(rel, binary, fp)

Relation rel;
bool binary;
FILE *fp;

{
    HeapTuple tuple, heap_formtuple();
    IndexTuple ituple, index_formtuple();
    Relation heap_openr();
    AttributeNumber attr_count;
    Attribute *attr;
    func_ptr *in_functions;
    int i, j, k, dummy;
    oid in_func_oid;
    Datum *values, *index_values;
    char *nulls, *index_nulls;
    bool *byval;
    Boolean isnull;
    bool has_index;
    int done = 0;
    char *string, *ptr, *CopyReadAttribute();
    Relation *index_rels;
    int32 len, null_ct, null_id;
    int32 ntuples, tuples_read = 0;
    bool reading_to_eof = true;
    ObjectId *elements;

    Relation *index_relations;
    int28 *index_atts;
    int n_indices;

    attr = (Attribute *) &rel->rd_att;

    attr_count = rel->rd_rel->relnatts;

    if (rel->rd_rel->relhasindex)
    {
        GetIndexRelations(rel->rd_id, &n_indices, &index_rels, &index_atts);
        has_index = true;
    }
    else
    {
        has_index = false;
    }

    if (!binary)
    {
        in_functions = (func_ptr *) palloc(attr_count * sizeof(func_ptr));
        elements = (ObjectId *) palloc(attr_count * sizeof(ObjectId));
        for (i = 0; i < attr_count; i++)
        {
            in_func_oid = (ObjectId) GetInputFunction(attr[i]->atttypid);
            fmgr_info(in_func_oid, &in_functions[i], &dummy);
            elements[i] = GetTypeElement(attr[i]->atttypid);
        }
    }
    else
    {
         fread(&ntuples, sizeof(int32), 1, fp);
         if (ntuples != 0) reading_to_eof = false;
    }

    values       = (Datum *) palloc(sizeof(Datum) * attr_count);
    index_values = (Datum *) palloc(sizeof(Datum) * attr_count);
    nulls        = (char *) palloc(attr_count);
    index_nulls  = (char *) palloc(attr_count);
    byval        = (bool *) palloc(attr_count * sizeof(bool));

    for (i = 0; i < attr_count; i++) 
    {
        nulls[i] = ' ';
        index_nulls[i] = ' ';
        byval[i] = (bool) IsTypeByVal(attr[i]->atttypid);
    }

    while (!done)
    {

        if (!binary)
        {
            for (i = 0; i < attr_count && !done; i++)
            {
                string = CopyReadAttribute(i, fp, &isnull);
                if (isnull)
                {
                    values[i] = NULL;
                    nulls[i] = 'n';
                }
                else if (string == NULL)
                {
                    done = 1;
                }
                else
                {
                    values[i] = (Datum) (in_functions[i]) (string, elements[i]);
		    if (attr[i]->attlen < 0 && values[i])
		    {
			if (VARSIZE(values[i]) > BLCKSZ)
			    elog(WARN, "attribute %d too long", i+1);
		    }
                }
            }
        }
        else /* binary */
        {
            fread(&len, sizeof(int32), 1, fp);
            if (feof(fp)) 
            {
                done = 1;
            }
            else
            {
                fread(&null_ct, sizeof(int32), 1, fp);
                if (null_ct > 0)
                {
                    for (i = 0; i < null_ct; i++)
                    {
                        fread(&null_id, sizeof(int32), 1, fp);
                        nulls[null_id] = 'n';
                    }
                }

                string = (char *) palloc(len);
                fread(string, len, 1, fp);

                ptr = string;

                for (i = 0; i < attr_count; i++)
                {
                    if (byval[i] && nulls[i] != 'n')
                    {
                        switch(attr[i]->attlen)
                        {
                            case 1:
                                values[i] = (Datum) *(unsigned char *) ptr;
                                ptr++;
                                break;
                            case 2:
                                values[i] = (Datum) *(unsigned short *) ptr; 
                                ptr = (char *) SHORTALIGN(ptr + 2);
                                break;
                            case 3:
                            case 4:
                                values[i] = (Datum) *(unsigned long *) ptr; 
                                ptr = (char *) LONGALIGN(ptr) + 4;
                                break;
                            default:
                                elog(WARN, "COPY BINARY: impossible size!");
                                break;
                        }
                    }
                    else if (nulls[i] != 'n')
                    {
                        if (attr[i]->attlen < 0)
                        {
                            values[i] = (Datum) (ptr + 4);
                            ptr = (char *)
                                  LONGALIGN(ptr + * (unsigned long *) ptr + 4);
                        }
                        else
                        {
                            values[i] = (Datum) ptr;
                            ptr = (char *) LONGALIGN(ptr) + attr[i]->attlen;
                        }
                    }
                }
            }
        }
        if (done) continue;

        tuple = heap_formtuple(attr_count, attr, values, nulls);
        heap_insert(rel, tuple, NULL);

        if (has_index)
        {
            for (i = 0; i < n_indices; i++)
            {
                j = 0;
                while (index_atts[i].data[j] != 0)
                {
                    if (nulls[index_atts[i].data[j] - 1] == 'n')
                    {
                        index_nulls[j] = 'n';
                    }
                    else 
                    {
                        index_values[j] = values[index_atts[i].data[j] - 1];
                    }
                    j++;
                }
                ituple = index_formtuple(j, &(index_rels[i]->rd_att.data[0]), 
                                         index_values, index_nulls);
                ituple->t_tid = tuple->t_ctid;
                (void) index_insert(index_rels[i], ituple, NULL, NULL);
                pfree(ituple);
                for (k = 0; k < j; k++) index_nulls[k] = ' ';
            }
        }

        if (binary) pfree(string);

        for (i = 0; i < attr_count; i++) 
        {
            if (!byval[i] && nulls[i] != 'n')
            {
                if (!binary) pfree(values[i]);
            }
            else if (nulls[i] == 'n')
            {
                nulls[i] = ' ';
            }
        }

		/* pfree the rulelock thing that is allocated */

		pfree(tuple->t_lock.l_lock);
        pfree(tuple);
        tuples_read++;

        if (!reading_to_eof && ntuples == tuples_read) done = true;
    }
    pfree(values);
    if (!binary) pfree(in_functions);
    pfree(nulls);
    pfree(byval);
    heap_close(rel);
}

GetOutputFunction(type)
    ObjectId    type;
{
    HeapTuple    typeTuple;

    typeTuple = SearchSysCacheTuple(TYPOID,
                    (char *) type,
                    (char *) NULL,
                    (char *) NULL,
                    (char *) NULL);

    if (HeapTupleIsValid(typeTuple))
    return((int) ((TypeTupleForm) GETSTRUCT(typeTuple))->typoutput);

    elog(WARN, "GetOutputFunction: Cache lookup of type %d failed", type);
    return(InvalidObjectId);
}

GetTypeElement(type)
    ObjectId    type;
{
    HeapTuple    typeTuple;

    typeTuple = SearchSysCacheTuple(TYPOID,
                    (char *) type,
                    (char *) NULL,
                    (char *) NULL,
                    (char *) NULL);

    if (HeapTupleIsValid(typeTuple))
    return((int) ((TypeTupleForm) GETSTRUCT(typeTuple))->typelem);

    elog(WARN, "GetOutputFunction: Cache lookup of type %d failed", type);
    return(InvalidObjectId);
}

GetInputFunction(type)
    ObjectId    type;
{
    HeapTuple    typeTuple;

    typeTuple = SearchSysCacheTuple(TYPOID,
                    (char *) type,
                    (char *) NULL,
                    (char *) NULL,
                    (char *) NULL);

    if (HeapTupleIsValid(typeTuple))
    return((int) ((TypeTupleForm) GETSTRUCT(typeTuple))->typinput);

    elog(WARN, "GetInputFunction: Cache lookup of type %d failed", type);
    return(InvalidObjectId);
}

IsTypeByVal(type)
    ObjectId    type;
{
    HeapTuple    typeTuple;

    typeTuple = SearchSysCacheTuple(TYPOID,
                    (char *) type,
                    (char *) NULL,
                    (char *) NULL,
                    (char *) NULL);

    if (HeapTupleIsValid(typeTuple))
        return((int) ((TypeTupleForm) GETSTRUCT(typeTuple))->typbyval);

    elog(WARN, "GetInputFunction: Cache lookup of type %d failed", type);

    return(InvalidObjectId);
}

/* 
 * Given the OID of a relation, return an array of index relation descriptors
 * and the number of index relations.  These relation descriptors are open
 * using heap_open().
 *
 * Space for the array itself is palloc'ed.
 */

#define N_INDEXRELS = 5

typedef struct rel_list
{
    ObjectId index_rel_oid;
    int28 intlist;
    struct rel_list *next;
}
RelationList;

GetIndexRelations(main_relation_oid, n_indices, index_rels, index_atts)

ObjectId main_relation_oid;
int *n_indices;
Relation **index_rels;
int28 **index_atts;

{
    RelationList *head, *scan;
    Relation pg_index_rel, heap_openr(), index_open();
    HeapScanDesc scandesc, heap_beginscan();
    ObjectId index_relation_oid;
    HeapTuple tuple;
    Attribute *attr;
    int i;
    int28 *datum;
    Boolean isnull;

    pg_index_rel = heap_openr("pg_index");
    scandesc = heap_beginscan(pg_index_rel, 0, NULL, NULL, NULL);
    attr = (Attribute *) &pg_index_rel->rd_att;

    *n_indices = 0;

    head = (RelationList *) palloc(sizeof(RelationList));
    scan = head;
    head->next = NULL;

    for (tuple = heap_getnext(scandesc, NULL, NULL);
         tuple != NULL; 
         tuple = heap_getnext(scandesc, NULL, NULL))
    {
        index_relation_oid = (ObjectId)
                         heap_getattr(tuple, InvalidBuffer, 2, attr, &isnull);
        if (index_relation_oid == main_relation_oid)
        {
            scan->index_rel_oid = (ObjectId) 
                heap_getattr(tuple,
			     InvalidBuffer,
			     Anum_pg_index_indexrelid,
			     attr,
			     &isnull);
            datum = (int28 *)
                heap_getattr(tuple,
			     InvalidBuffer,
			     Anum_pg_index_indkey,
			     attr,
			     &isnull);
            bcopy(datum, &scan->intlist, sizeof(int28));
            (*n_indices)++;
            scan->next = (RelationList *) palloc(sizeof(RelationList));
            scan = scan->next;
        }
    }

    heap_endscan(scandesc);
    heap_close(pg_index_rel);

    *index_rels = (Relation *) palloc(*n_indices * sizeof(Relation));
    *index_atts = (int28 *) palloc(*n_indices * sizeof(int28));

    for (i = 0, scan = head; i < *n_indices; i++, scan = scan->next)
    {
        (*index_rels)[i] = index_open(scan->index_rel_oid);
        bcopy(&scan->intlist, &((*index_atts)[i]), sizeof(int28));
    }

    for (i = 0, scan = head; i < *n_indices + 1; i++)
    {
        scan = head->next;
        pfree(head);
        head = scan;
    }
}


#define EXT_ATTLEN 2*8192

/*
 * Reads input from fp until eof is seen.  If we are reading from standard
 * input, AND we see a dot on a line by itself (a dot followed immediately
 * by a newline), we exit as if we saw eof.  This is so that copy pipelines
 * can be used as standard input.
 */

char *
CopyReadAttribute(attno, fp, isnull)

int attno;
FILE *fp;
Boolean *isnull;

{
    static char attribute[EXT_ATTLEN];
    char c;
    int done = 0;
    int i = 0;
    int length = 0;
    int read_newline = 0;

    if (feof(fp))
    {
        *isnull = (Boolean) false;
        return(NULL);
    }

    while (!done)
    {
        c = getc(fp);

        if (feof(fp))
        {
            *isnull = (Boolean) false;
            return(NULL);
        }
        else if (reading_from_input && attno == 0 && i == 0 && c == '.') 
        {
            attribute[0] = c;
            c = getc(fp);
            if (c == '\n')
            {
                *isnull = (Boolean) false;
                return(NULL);
            }
            else if (c == '\t')
            {
                attribute[1] = 0;
                *isnull = (Boolean) false;
                return(&attribute[0]);
            }
            else
            {
                attribute[1] = c;
                i = 2;
            }
        }
        else if (c == '\\') 
        {
            c = getc(fp);
        }
        else if (c == '\t' || c == '\n')
        {
            done = 1;
        }
        if (!done) attribute[i++] = c;
        if (i == EXT_ATTLEN - 1)
            elog(WARN, "CopyReadAttribute - attribute length too long");
    }
    attribute[i] = '\0';
    if (i == 0) 
    {
        *isnull = (Boolean) true;
        return(NULL);
    }
    else
    {
        *isnull = (Boolean) false;
        return(&attribute[0]);
    }
}

CopyAttributeOut(fp, string)

FILE *fp;
char *string;

{
    int i;
    int len = strlen(string);

    for (i = 0; i < len; i++)
    {
        if (string[i] == '\t' || string[i] == '\\')
        {
            fputc('\\', fp);
        }
        fputc(string[i], fp);
    }
}

/*
 * Returns the number of tuples in a relation.  Unfortunately, currently
 * must do a scan of the entire relation to determine this.
 *
 * relation is expected to be an open relation descriptor.
 */

int
CountTuples(relation)

Relation relation;

{
    HeapScanDesc scandesc, heap_beginscan();
    HeapTuple tuple, heap_getnext();

    int i;

    scandesc = heap_beginscan(relation, 0, NULL, NULL, NULL);

    for (tuple = heap_getnext(scandesc, NULL, NULL), i = 0;
         tuple != NULL; 
         tuple = heap_getnext(scandesc, NULL, NULL), i++);
    heap_endscan(scandesc);
    return(i);
}
