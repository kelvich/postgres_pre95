/*
 * "$Header$"
 */

#include <stdio.h>

#include "tmp/postgres.h"
#include "catalog/syscache.h"
#include "catalog/pg_type.h"

#include "access/heapam.h"
#include "access/htup.h"
#include "access/relscan.h"
#include "utils/rel.h"
#include "utils/log.h"
#include "utils/dynamic_loader.h"
#include "tmp/daemon.h"

void
DoCopy(relname, binary, from, pipe, filename)

char *relname;
bool binary, from, pipe;
char *filename;

{
    FILE *fp;

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
    if (from && pipe) fflush(stdin);
}

CopyTo(relname, binary, fp)

char *relname;
bool binary;
FILE *fp;

{
    HeapTuple tuple, amgetnext();
    Relation rel, amopenr();
    HeapScanDesc scandesc, ambeginscan();
    AttributeNumber attr_count;
    Attribute *attr;
    func_ptr *out_functions;
    int i, dummy, entering = 1;
    oid out_func_oid;
    Datum value;
    Boolean isnull = (Boolean) true;
    char *string;

    rel = amopenr(relname);
    if (rel == NULL) elog(WARN, "%s: class %s does not exist", relname);
    scandesc = ambeginscan(rel, 0, NULL, NULL, NULL);

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

    for (tuple = amgetnext(scandesc, NULL, NULL);
         tuple != NULL; 
         tuple = amgetnext(scandesc, NULL, NULL))
    {
        for (i = 0; i < attr_count; i++)
        {
            value = (Datum) 
                    amgetattr(tuple, InvalidBuffer, i+1, attr, &isnull);
            if (!binary)
            {
                string = (char *) (out_functions[i]) (value);
                CopyAttributeOut(fp, string);
                if (i == attr_count - 1)
                {
                    fputc('\n', fp);
                }
                else
                {
                    fputc('\t', fp);
                }
                pfree(string);
            }
        }
    }

    amendscan(scandesc);
    pfree(out_functions);
    amclose(rel);
    fclose(fp);
}

CopyFrom(relname, binary, fp)

char *relname;
bool binary;
FILE *fp;

{
    HeapTuple tuple, formtuple();
    Relation rel, amopenr();
    AttributeNumber attr_count = 1;
    Attribute *attr, *ptr;
    func_ptr *in_functions;
    int i, dummy, entering = 1;
    oid in_func_oid;
    Datum *values;
    char *nulls;
    bool *byval;
    Boolean isnull;
    int done = 0;
    char *string, *CopyReadAttribute();

    rel = amopenr(relname);
    if (rel == NULL) elog(WARN, "%s: class %s does not exist", relname);

    attr = (Attribute *) &rel->rd_att;

    attr_count = rel->rd_rel->relnatts;

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

        for (i = 0; i < attr_count; i++)
        {
            string = CopyReadAttribute(fp, &isnull);
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

        if (done) continue;

        tuple = formtuple(attr_count, attr, values, nulls);
        aminsert(rel, tuple, NULL);
        for (i = 0; i < attr_count; i++) 
        {
            if (!byval[i] && nulls[i] != 'n')
            {
                pfree(values[i]);
            }
            else if (nulls[i] == 'n')
            {
                nulls[i] = ' ';
            }
        }
        pfree(tuple);
    }
    pfree(values);
    pfree(in_functions);
    pfree(nulls);
    pfree(byval);
    amclose(rel);
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

char *
CopyReadAttribute(fp, isnull)

FILE *fp;
Boolean *isnull;

{
    static char attribute[ATTLEN];
    char c;
    int done = 0;
    int i = 0;
    int length = 0;

    while (!done)
    {
        c = getc(fp);
        if (feof(fp))
        {
            done = 1;
            *isnull = (Boolean) false;
            return(NULL);
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
