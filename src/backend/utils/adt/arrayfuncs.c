/*
 * arrayfuncs.c --
 *     Special array in and out functions for arrays.
 *
 *
 * RcsId("$Header$");
 */

#include <ctype.h>
#include "tmp/postgres.h"
#include "tmp/align.h"

#include "catalog/pg_type.h"
#include "catalog/syscache.h"

#include "utils/palloc.h"
#include "utils/fmgr.h"
#include "utils/log.h"

/*
 * Function prototypes - contained in this file as no one outside should
 * be calling these except the function manager.
 */

int array_count ARGS((char *string , char delimiter ));
int system_cache_lookup ARGS((
    ObjectId
    element_type ,
    Boolean input ,
    int *typlen ,
    bool *typbyval ,
    char *typdelim ,
    ObjectId *typelem ,
    ObjectId *proc
));

/*
 *    array_count - counts the number of elements in an array.
 */

int
array_count(string, delimiter)

char *string;
char delimiter;

{
    int i = 1, nelems = 0;
    bool scanning_string = false;
    int nest_level = 1;

    while (nest_level >= 1)
    {
        switch (string[i])
        {
            case '\\':
                i++;
                break;
            case '{':
                if (!scanning_string) nest_level++;
                break;
            case '}':
                if (!scanning_string) nest_level--;
                break;
            case '\"':
                scanning_string = !scanning_string;
                break;
	    case 0:
		elog(WARN, "array constant is missing a closing bracket");
		break;
            default:
                if (!scanning_string
                  && nest_level == 1
                  && string[i] == delimiter)
                {
                    nelems++;
                }
                break;
        }
        i++;
    }

    /*
     * account for last element in list.
     */

    return(nelems + 1);
}

/*
 *    array_in - takes an array surrounded by {...} and returns it in
 *    VARLENA format:
 */

char *
array_in(string, element_type)

char *string;
ObjectId element_type;

{
    static ObjectId charTypid = InvalidObjectId;
    int typlen;
    bool typbyval;
    char typdelim;
    ObjectId typinput;
    ObjectId typelem;
    char *string_save, *p, *q, *r;
    char **values;
    func_ptr inputproc;
    int i, nitems, dummy;
    int32 nbytes;
    char *retval;
    bool scanning_string = false;

    if (charTypid == InvalidObjectId)
    {
	HeapTuple tup;

	tup = (HeapTuple)type("char");
	if ((charTypid = tup->t_oid) == InvalidObjectId)
	    elog(WARN, "type lookup on char failed");
    }

    system_cache_lookup(element_type, true, &typlen, &typbyval,
                        &typdelim, &typelem, &typinput);

    fmgr_info(typinput, & inputproc, &dummy);

    string_save = (char *) palloc(strlen(string) + 3);

    if (string[0] != '{')
    {
	if (element_type == charTypid)
	{
	    /*
	     * this is a hack to allow char arrays to have a { in the
	     * first position.  Of course now \{ can never be the first
	     * two characters, but what else can we do?
	     */
	    string =
		(*string == '\\' && *(string+1) == '{') ? &string[1] : string;
	    return (char *)textin(string);
	}
	else
	    elog(WARN, "array_in:  malformed array constant");
    }
    else
    {
        strcpy(string_save, string);
    }

    nitems = array_count(string, typdelim);

    values        = (char **) palloc(nitems * sizeof(char *));

    p = q = string_save;

    p++; q++; /* get past leading '{' */

    for (i = 0; i < nitems; i++)
    {
        int nest_level = 0;
        bool done = false;
	bool eoArray = false;

        while (!done)
        {
            switch (*q)
            {
                case '\\':
                    /* Crunch the string on top of the backslash. */
                    for (r = q; *r != '\0'; r++) *r = *(r+1);
                    break;
                case '\"':
                    if (!scanning_string && nest_level == 0)
                    {
                        scanning_string = true;
                        while (p != q) p++;
                        p++; /* get p past first doublequote */
                        break;
                    }
                    else if (nest_level == 0)
                    {
                        scanning_string = false;
                        *q = '\0';
                    }
                    break;
                case '{':
                    if (!scanning_string) nest_level++;
                    break;
                case '}':
                    if (nest_level == 0 && !scanning_string)
		    {
                        done = true;
			eoArray = true;
		    }
                    else if (!scanning_string) nest_level--;
                    break;
                default:
                    if (*q == typdelim && nest_level == 0) done = true;
                    break;
            }
            if (!done) q++;
        }
        *q = '\0';                    /* Put a null at the end of it */
        values[i] = (*inputproc) (p, typelem);
	p = ++q;	/* p goes past q */
	if (!eoArray)	/* if not at the end of the array skip white space */
	    while (isspace(*q))
	    {
		p++;
		q++;
	    }
    }

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
    pfree((char *)values);
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

    int typlen;
    bool typbyval;
    char typdelim;
    ObjectId typoutput;
    ObjectId typelem;

    char *p;
    char *retval;
    char **values;
    int32 nitems, nbytes, overall_length;
    int i, dummy;
    func_ptr outputproc;
    char delim[2];

    system_cache_lookup(element_type, false, &typlen, &typbyval,
                        &typdelim, &typelem, &typoutput);

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
        nbytes = (* (int32 *) items) - sizeof(int32);
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

        /*
         * So that the mapping between input and output functions is preserved
         * in the case of array_out(array_in(string)), we have to put double
         * quotes around strings that look like text strings.  To be sure, and
         * since it will not break anything, we will put double quotes around
         * anything that is not passed by value.
         */

        if (!typbyval) overall_length += 2;
    }

    p = (char *) palloc(overall_length + 3);
    retval = p;

    strcpy(p, "{");

    for (i = 0; i < nitems; i++)
    {
        /*
         * Surround anything that is not passed by value in double quotes.
         * See above for more details.
         */

        if (!typbyval)
        {
            strcat(p, "\"");
            strcat(p, values[i]);
            strcat(p, "\"");
        }
        else
        {
            strcat(p, values[i]);
        }

        if (i != nitems - 1) strcat(p, delim); else strcat(p, "}");
        pfree(values[i]);
    }

    pfree((char *)values);

    return(retval);
}

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
