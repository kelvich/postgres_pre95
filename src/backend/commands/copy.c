/*
 * "$Header$"
 */

#include <stdio.h>

#include "tmp/postgres.h"
#include "tmp/align.h"
#include "catalog/syscache.h"
#include "catalog/pg_type.h"

#include "access/heapam.h"
#include "access/htup.h"
#include "access/relscan.h"
#include "utils/rel.h"
#include "utils/log.h"
#include "utils/dynamic_loader.h"
#include "tmp/daemon.h"

/*
 * New copy code.
 * 
 * This code "knows" the following about tuples:
 * 
 */

static bool reading_from_input = false;

void
DoCopy(relname, binary, from, pipe, filename)

char *relname;
bool binary, from, pipe;
char *filename;

{
    FILE *fp;

    reading_from_input = pipe;

    if (from)
    {
        fp = pipe ? stdin : fopen(filename, "r");
        if (fp == NULL) 
        {
            elog(WARN, "COPY: file %s could not be open for reading", filename);
        }
        CopyFrom(relname, binary, fp);
    }
    else
    {
        fp = pipe ? stdout : fopen(filename, "w");
        if (fp == NULL) 
        {
            elog(WARN, "COPY: file %s could not be open for writing", filename);
        }
        CopyTo(relname, binary, fp);
    }
    if (!pipe)
    {
        fclose(fp);
    }
    else if (from)
    {
        fflush(stdin);
    }
}

CopyTo(relname, binary, fp)

char *relname;
bool binary;
FILE *fp;

{
    HeapTuple tuple, heap_getnext();
    Relation rel, heap_openr();
    HeapScanDesc scandesc, heap_beginscan();

    int32 attr_count, i;
    Attribute *attr;
    func_ptr *out_functions;
    int dummy;
    oid out_func_oid;
    Datum value;
    Boolean isnull = (Boolean) true;
    char *nulls;
    char *string;
    bool *byval;

    rel = heap_openr(relname);
    if (rel == NULL) elog(WARN, "%s: class %s does not exist", relname);
    scandesc = heap_beginscan(rel, 0, NULL, NULL, NULL);

    attr_count = rel->rd_rel->relnatts;
    attr = (Attribute *) &rel->rd_att;

    if (!binary)
    {
        out_functions = (func_ptr *)
                         palloc(attr_count * sizeof(func_ptr));
        for (i = 0; i < attr_count; i++) {
            out_func_oid = (oid) GetOutputFunction(attr[i]->atttypid);
            fmgr_info(out_func_oid, &out_functions[i], &dummy);
        }
    }
    else
    {
        nulls = (char *) palloc(attr_count);
        for (i = 0; i < attr_count; i++) nulls[i] = ' ';
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
                    string = (char *) (out_functions[i]) (value);
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
    }

    heap_close(rel);
}

CopyFrom(relname, binary, fp)

char *relname;
bool binary;
FILE *fp;

{
    HeapTuple tuple, heap_formtuple();
    Relation rel, heap_openr();
    AttributeNumber attr_count;
    Attribute *attr;
    func_ptr *in_functions;
    int i, dummy;
    oid in_func_oid;
    Datum *values;
    char *nulls;
    bool *byval;
    Boolean isnull;
    int done = 0;
    char *string, *ptr, *CopyReadAttribute();
    int32 len, null_ct, null_id;

    Relation *index_relations;

    rel = heap_openr(relname);
    if (rel == NULL) elog(WARN, "%s: class %s does not exist", relname);

    attr = (Attribute *) &rel->rd_att;

    attr_count = rel->rd_rel->relnatts;

    if (rel->rd_rel->relhasindex)
    {
    }
    
    if (!binary)
    {
        in_functions = (func_ptr *) palloc(attr_count * sizeof(func_ptr));
           for (i = 0; i < attr_count; i++)
        {
            in_func_oid = (oid) GetInputFunction(attr[i]->atttypid);
            fmgr_info(in_func_oid, &in_functions[i], &dummy);
        }
    }

    values = (Datum *) palloc(sizeof(Datum) * attr_count);
    nulls = (char *) palloc(attr_count);
    byval = (bool *) palloc(attr_count * sizeof(bool));

    for (i = 0; i < attr_count; i++) 
    {
        nulls[i] = ' ';
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
                    values[i] = (Datum) (in_functions[i]) (string);
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
        pfree(tuple);
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

#define ATTLEN 2048 /* need to fix if attributes ever get very long */

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
    static char attribute[ATTLEN];
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
        if (i == ATTLEN - 1)
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
