/*
 * arrayfuncs.c --
 *     Special array in and out functions for arrays.
 *
 *
 * RcsId("$Header$");
 */

#include "tmp/postgres.h"
#include "tmp/align.h"

#include "catalog/pg_type.h"
#include "catalog/syscache.h"

#include "utils/palloc.h"
#include "utils/fmgr.h"
#include "utils/log.h"

/*
 *    array_in - takes an array surrounded by {...} and returns it in
 *    VARLENA format:
 */

char *
array_in(string, element_type)

char *string;
ObjectId element_type;

{
    static ObjectId element_type_save = 0;
    static int typlen;
    static bool typbyval;
    static char typdelim;
    static ObjectId typinput;
    static ObjectId typelem;

    char *string_save, *p, *q;
    char **values;
    FmgrFunction inputproc;
    int i, nitems, dummy;
    int32 nbytes;
    char *retval;

    string_save = (char *) palloc(strlen(string) + 3);
    if (*string != '{')
    {
        sprintf(string_save, "{%s}", string);
    }
    else
    {
        strcpy(string_save, string);
    }

    if (element_type_save != element_type)
    {
        system_cache_lookup(element_type, true, &typlen, &typbyval,
                            &typdelim, &typelem, &typinput);
        element_type_save = element_type;
    }

    fmgr_info(typinput, & inputproc, &dummy);

    for (i = 0, nitems = 0; string_save[i] != '}'; i++) 
    {
        if (string_save[i] == typdelim) nitems++; 
    }

    nitems++; /* account for last item in list */

    values = (char **) palloc(nitems * sizeof(char *));

    p = q = string_save;

    p++; q++; /* get past leading '{' */

    for (i = 0; i < nitems - 1; i++)
    {
        while (*q != typdelim) q++;   /* Get to end of next string */
        *q = '\0';                    /* Put a null at the end of it */
        values[i] = (*inputproc) (p, typelem);
        p = q + 1; q++;               /* p goes past q */
    }

    while (*q != '}') q++;
    *q = '\0';

    values[nitems - 1] = (*inputproc) (p, typelem);

    if (typlen > 0)
    {
        nbytes = nitems * typlen + sizeof(int32);
    }
    else
    {
        for (i = 0, nbytes = 0;
             i < nitems;
             nbytes += LONGALIGN(* (int32 *) values[i]), i++);
        nbytes += sizeof(int32);
    }

    retval = (char *) palloc(nbytes);
    p = retval + 4;

    bcopy(&nbytes, retval, sizeof(int4));

    for (i = 0; i < nitems; i++)
    {
        if (typlen > 0)
        {
            if (typbyval)
            {
                bcopy(&values[i], p, typlen);
            }
            else
                bcopy(values[i], p, typlen);
            p += typlen;
        }
        else
        {
            int len;

            len = LONGALIGN(* (int32 *) values[i]);
            bcopy(values[i], p, * (int32 *) values[i]);
            p += len;
        }
        if (!typbyval) pfree(values[i]);
    }
    pfree(string_save);
    pfree(values);
    return(retval);
}

char *
array_out(items, element_type)

char *items;
ObjectId element_type;

{
    /*
     * statics so we don't do excessive system cache lookups.
     */

    static ObjectId element_type_save = 0;
    static int typlen;
    static bool typbyval;
    static char typdelim;
    static ObjectId typoutput;
    static ObjectId typelem;

    char *p;
    char *retval;
    char **values;
    int32 nitems, nbytes, overall_length;
    int i, dummy;
    FmgrFunction outputproc;
    char delim[2];

    if (element_type != element_type_save)
    {
        system_cache_lookup(element_type, false, &typlen, &typbyval,
                            &typdelim, &typelem, &typoutput);
        element_type_save = element_type;
    }

    fmgr_info(typoutput, & outputproc, &dummy);
    sprintf(delim, "%c", typdelim);

    /*
     * It's an array of fixed-length things (either fixed-length arrays
     * or non-array objects.  We can compute the number of items just by
     * dividing the number of bytes in the blob of memory by the length
     * of each element.
     */

    if (typlen > 0)
    {
        nitems = (* (int32 *) items - 4)/ typlen;
    }
    else

    /*
     * It's an array of variable length objects.  We have to manually walk
     * through each variable length element to count the number of elements.
     */

    {
        nbytes = * (int32 *) items - sizeof(int32);
        nitems = 0;
        p = items + sizeof(int32);

        while (nbytes != 0)
        {
            nbytes -= LONGALIGN(* (int32 *) p);
            p += LONGALIGN(* (int32 *) p);
            nitems++;
        }
    }

    items += sizeof(int32);

    values = (char **) palloc(nitems * sizeof (char *));
    overall_length = 0;

    for (i = 0; i < nitems; i++)
    {
        if (typbyval)
        {
            switch(typlen)
            {
                case 1:
                    values[i] = (*outputproc) (*items, typelem);
                    break;
                case 2:
                    values[i] = (*outputproc) (* (int16 *) items, typelem);
                    break;
                case 3:
                case 4:
                    values[i] = (*outputproc) (* (int32 *) items, typelem);
                    break;
            }
            items += typlen;
        }
        else
        {
            values[i] = (*outputproc) (items, typelem);
            if (typlen > 0)
                items += typlen;
            else
                items += LONGALIGN(* (int32 *) items);
        }
        overall_length += strlen(values[i]);
    }

    p = (char *) palloc(overall_length + 3);
    retval = p;

    strcpy(p, "{");

    for (i = 0; i < nitems - 1; i++)
    {
        strcat(p, values[i]);
        strcat(p, delim);
        pfree(values[i]);
    }

    strcat(p, values[nitems - 1]);
    strcat(p, "}");

    pfree(values[nitems - 1]);
    pfree(values);

    return(retval);
}

char *
string_in(a1, a2) {}

char *
string_out(a1, a2) {}

system_cache_lookup(element_type, input, typlen, typbyval, typdelim,
                    typelem, proc)

ObjectId element_type;
Boolean input;
int *typlen;
bool *typbyval;
char *typdelim;
ObjectId *typelem;
ObjectId *proc;

{
    HeapTuple typeTuple;
    TypeTupleForm typeStruct;

    typeTuple = SearchSysCacheTuple(TYPOID, element_type, NULL, NULL, NULL);

    if (!HeapTupleIsValid(typeTuple))
    {
        elog(WARN, "array_out: Cache lookup failed for type %d\n",
             element_type);
        return NULL;
    }
    typeStruct = (TypeTupleForm) GETSTRUCT(typeTuple);
    *typlen    = typeStruct->typlen;
    *typbyval  = typeStruct->typbyval;
    *typdelim  = typeStruct->typdelim;
    *typelem   = typeStruct->typelem;
    if (input)
    {
        *proc = typeStruct->typinput;
    }
    else
    {
        *proc = typeStruct->typoutput;
    }
}
