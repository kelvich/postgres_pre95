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
#include "tmp/libpq-fs.h"

#include "catalog/pg_type.h"
#include "catalog/syscache.h"
#include "catalog/pg_lobj.h"

#include "utils/palloc.h"
#include "fmgr.h"
#include "utils/log.h"
#include "array.h"

char * _ReadLOArray(), * _ReadArrayString(), * array_seek(), 
     * _array_newLO();
Datum _ArrayCast();

/* An array has the following internal structure:
 *	<nbytes>			- total number of bytes 
 * 	<ndim>				- number of dimensions of the array 
 *	<is_largeObject>	- boolean flag to idicate if array is LO-type
 *	<dim>				- size of each array axis 
 *	<dim_lower>			- lower boundary of each dimension 
 *	<actual data>		- whatever is the stored data
*/

/*-=-=--=-=-=-=-=-=-=-=--=-=-=-=-=-=-=-=--=-=-=-=-=-=-=-=--=-=-=-=-=-=-=-*/

char *
array_in(string, element_type)
char *string;
ObjectId element_type;

{
    static ObjectId charTypid = InvalidObjectId;
    int typlen;
    bool typbyval, done, isLO = false;
    char typdelim;
    ObjectId typinput;
    ObjectId typelem;
    char *string_save, *p, *q, *r;
    func_ptr inputproc;
    int i, nitems, dummy;
    int32 nbytes;
    char *dataPtr;
	ArrayType *retval;
	int ndim, dim[MAXDIM], lBound[MAXDIM];

    system_cache_lookup(element_type, true, &typlen, &typbyval,
                        &typdelim, &typelem, &typinput);

    fmgr_info(typinput, & inputproc, &dummy);

    string_save = (char *) palloc(strlen(string) + 3);
    strcpy(string_save, string);

/* --- read array dimensions  ---------- */
	p = q = string_save; done = false;
    for (ndim = 0; !done; ){
        while (isSpace(*p)) p++;
        switch (*p) {
            case '[':
				p++;
				if ((r = (char *)index(p, ':')) == (char *)NULL)
					lBound[ndim] = 1;
				else { *r = '\0'; 
						lBound[ndim] = atoi(p); 
						p = r + 1;
				}
                for (q = p; isNumber(*q); q++);
                if (*q != ']')
                  elog(WARN, "array_in: missing ']' in array declaration");
                *q = '\0';
                dim[ndim] = atoi(p);
				if ((dim[ndim] < 0) || (lBound[ndim] < 0))
	    		elog(WARN,"array_in: array dimensions need to be positive");
				dim[ndim] = dim[ndim] - lBound[ndim] + 1;
				if (dim[ndim] < 0)
				elog(WARN, "array_in: upper bound cannot be less than lower bound");
                p = q + 1; ndim++;
                break;
            default:
                done = true;
                break;
        }
    }
    if (ndim == 0) {
	if (*p == '{') {
		ndim = _ArrayCount(p, dim, typdelim);
		if (ndim == 0) 
		elog(WARN, "array_in: Cannot determine array dimensions from input");
		for (i = 0; i < ndim; lBound[i++] = 1);
	} else 
		elog(WARN, "array_in: Need to specify array dimension or enclose within '{' '}'");
	}
	else {
		while (isSpace(*p)) p++;
    	if (*p == '\0')  elog(WARN, "array_in: missing assignment operator");
    	if (*p != '=') elog(WARN, "array_in: missing assignment operator");
    	p++;
    	while (isSpace(*p)) p++;
	}		

	getNitems(nitems, ndim, dim);
    if (nitems == 0)
    {
	char *emptyArray = palloc(sizeof(ArrayType));
	bzero(emptyArray, sizeof(ArrayType));
	* (int32 *) emptyArray = sizeof(ArrayType);
	return emptyArray;
    }

	if (*p == '{') dataPtr = (char *)
		_ReadArrayString(p, nitems, inputproc, typelem,typdelim, 
							typlen, typbyval, &nbytes );
	else  {
		int dummy;
		dataPtr = (char *)_ReadLOArray(p, &nbytes, &dummy);
		isLO = true;
	}

	nbytes += ARR_OVERHEAD(ndim);
	retval = (ArrayType *) palloc(nbytes);
	
	bzero (retval, nbytes);
	bcopy(&nbytes, retval, sizeof(int));
	bcopy(&ndim, ARR_NDIM_PTR(retval), sizeof(int));
	bcopy(&isLO, ARR_IS_LO_PTR(retval), sizeof(bool));
	bcopy(dim, ARR_DIMS(retval), ndim*sizeof(int));
	bcopy(lBound, ARR_LBOUND(retval), ndim*sizeof(int));
	_CopyArrayValues(isLO, dataPtr, ARR_DATA_PTR(retval), 
						nitems, typlen, typbyval);
	pfree(string_save);
	return((char *)retval);
}

/*-==-=-=-=-=--=-=-=-=-=-=-=-=--=-=-=-=-=-=-=-=--=-=-=-=-=-=-=-=--=-=-=-=-=-=-*/
/* Counts the number of dimensions and the dim[] array for an array string. The 
 * syntax for array input is C-like nested curly braces 
 *----------------------------------------------------------------------------*/
int _ArrayCount(str, dim, typdelim)
int dim[], typdelim;
char *str;
{
	int nest_level = 0, i; 
	int ndim = 0, temp[MAXDIM];
	bool scanning_string = false;
	bool eoArray = false;
	char *q;

	for (i = 0; i < MAXDIM; temp[i] = dim[i++] = 0);
	q = str;

	if (strncmp (str, "{}", 2) == 0) return(0); 

	while (eoArray != true) {
        bool done = false;
        while (!done)
        {
            switch (*q)
            {
                case '\"':
                    if (!scanning_string )
                        scanning_string = true;
                    else
                        scanning_string = false;
                    break;
                case '{':
                    if (!scanning_string)  { 
						temp[nest_level] = 0;
						nest_level++;
					}
                    break;
                case '}':
                    if (!scanning_string) {
						if (!ndim) ndim = nest_level;
                    	nest_level--;
						if (nest_level) temp[nest_level-1]++;
						if (nest_level == 0) eoArray = done = true;
					}
                    break;
                default:
					if (!ndim) ndim = nest_level;
                    if (*q == typdelim && !scanning_string )
						done = true;
                    break;
            }
            if (!done) q++;
        }
		temp[ndim-1]++;
		q++;
		if (!eoArray) while (isspace(*q)) q++;
    }
	for (i = 0; i < ndim; dim[i] = temp[i++]);
	return(ndim);
}

/*-=-=-=-=-=-=-=-=--=-=-=-=-=-=--=-=-=-=-=-=-=-=--=-=-=-=-=-=-=-=--=-=-=-=-=-=-*/
/* parses the array string pointed by "arrayStr and converts it in the internal
 * format. inputproc() is the pointer to the function used for the conversion
 * The number of bytes used for storing the string is returned in "nbytes
 *-----------------------------------------------------------------------------*/
char *
_ReadArrayString (arrayStr, nitems, inputproc, typelem, typdelim, typlen, typbyval, nbytes)
int nitems, typlen, *nbytes;
ObjectId typelem;
char typdelim, *arrayStr;
func_ptr inputproc;
bool typbyval;
{
	int i, dummy, nest_level = 0;
    char *p, *q, *r, **values;
    bool scanning_string = false;

	*nbytes = 0;
	if (strcmp(arrayStr, "{}") == 0) {
		/* empty array, just return the total number of bytes */
    	if (typlen > 0) *nbytes = nitems * typlen;
    	else elog(WARN, "array_in: Empty initialization not possible on variable length array elements");
		return(NULL);
	}

	/* read array enclosed within {} */	
    values = (char **) palloc(nitems * sizeof(char *));
	q = p = arrayStr;

    for (i = 0; i < nitems; i++)
    {
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
                    if (!scanning_string )
                    {
                        scanning_string = true;
                        while (p != q) p++;
                        p++; /* get p past first doublequote */
                        break;
                    }
                    else
                    {
                        scanning_string = false;
                        *q = '\0';
                    }
                    break;
                case '{':
                    if (!scanning_string) { p++; nest_level++;}
                    break;
                case '}':
                    if (!scanning_string) {
                    	nest_level--;
						if (nest_level == 0) { 
							eoArray = done = true;
							if (i != nitems - 1)
							 elog(WARN, "array_in: array dimensions don't match with array input"); 
						}
						else  *q = '\0';
					}
                    break;
                default:
                    if (*q == typdelim && !scanning_string )
					done = true;
                    break;
            }
            if (!done) q++;
        }
        *q = '\0';                    /* Put a null at the end of it */
        values[i] = (*inputproc) (p, typelem);
	if (!typbyval && !values[i])
	{
	    elog(NOTICE, "pass by reference array element is NULL, you may");
	    elog(WARN, "need to quote each individual element in the constant");
	}
	p = ++q;	/* p goes past q */
	if (!eoArray)	/* if not at the end of the array skip white space */
	    while (isspace(*q))
	    {
		p++;
		q++;
	    }
    }
    if (typlen > 0) *nbytes = nitems * typlen;
    else
        for (i = 0, *nbytes = 0; i < nitems;
             *nbytes += LONGALIGN(* (int32 *) values[i]), i++);
	return((char *)values);
}


/*****************************************************************************
 * Read data about an array to be stored as a large object                    
 */
char *
_ReadLOArray(name, nbytes, fd)
char *name;
int *nbytes, *fd;
{
	char *saveName;

	while (isSpace(*name))
        name++;
	saveName = name;
	while (!(isSpace(*name)) && (*name != '\0'))
        name++;
	*name = '\0';
	name = palloc(strlen(saveName) + 2);
	strcpy (name, saveName);
	if (FilenameToOID(name) == InvalidObjectId)
        if ( (*fd = LOcreat (name, 0600, Unix)) < 0)
            elog(WARN, "Large object create failed");
	pfree(name);
	*nbytes = strlen(saveName) + 2;
	return(saveName);
}

/*-=-=-=-==-=-=-=--=-=-=-=-=-=-=-=--=-=-=-=-=-=-=-=--=-=-=-=-=--=-=-=-=-=-=-=-*/
_CopyArrayValues(typFlag, values, p, nitems, typlen, typbyval)
bool typFlag, typbyval;
int nitems, typlen;
char *p, **values;
{
	int i;

	if (typFlag == false) {
		/* Regular array , so copy the actual array value */

		if (values == NULL) {
		/* Empty array, just allocate the required space and zero it */
		if (typlen > 0)	bzero (p, nitems*typlen);
		return;
		}
    	for (i = 0; i < nitems; i++)
    	{
			int inc;
			inc = ArrayCastAndSet((char *)values[i], typbyval, typlen, p);
			p += inc;
			if (!typbyval) pfree((char *)values[i]);
		}
    	pfree(values);
	} 
	else {
	   /* It is a large object, so just copy the name of the large object 
		that holds the actual data */
	   strcpy(p, values);			
	}
}

/*******************************************************************************/
/* array_out()
 */
char *
array_out(v, element_type)
ArrayType *v;
ObjectId element_type;
{

    if (v == (ArrayType *) NULL)
        return ((char *) NULL);

	if (ARR_IS_LO(v) == true)  {
    char *p, *save_p;
    int nbytes, i;

    /* get a wide string to print to */
    nbytes = strlen(ARR_DATA_PTR(v)) + 4;

    save_p = (char *) palloc(nbytes);

    sprintf(save_p, "%s ", ARR_DATA_PTR(v));
    return (save_p);
	}

	else { 
	int typlen;
    bool typbyval;
    char typdelim;
    ObjectId typoutput;
    ObjectId typelem;

    char *p, *q;
    char *retval;
    char **values;
    int nitems, nbytes, overall_length;
    int i, dummy;
    func_ptr outputproc;
    char delim[2];
    int ndim, *dim, *lBound;

	system_cache_lookup(element_type, false, &typlen, &typbyval,
                        &typdelim, &typelem, &typoutput);
	fmgr_info(typoutput, & outputproc, &dummy);
    sprintf(delim, "%c", typdelim);
	ndim = ARR_NDIM(v);
	dim = ARR_DIMS(v);
	getNitems(nitems, ndim, dim);

	if (nitems == 0)
    {
    char *emptyArray = palloc(3);
    emptyArray[0] = '{';
    emptyArray[1] = '}';
    emptyArray[2] = '\0';
    return emptyArray;
    }

	p = ARR_DATA_PTR(v);
    overall_length = 0;
    values = (char **) palloc(nitems * sizeof (char *));
    for (i = 0; i < nitems; i++)
    {
        if (typbyval)
        {
            switch(typlen)
            {
                case 1:
                    values[i] = (*outputproc) (*p, typelem);
                    break;
                case 2:
                    values[i] = (*outputproc) (* (int16 *) p, typelem);
                    break;
                case 3:
                case 4:
                    values[i] = (*outputproc) (* (int32 *) p, typelem);
                    break;
            }
            p += typlen;
        }
        else
        {
            values[i] = (*outputproc) (p, typelem);
            if (typlen > 0)
                p += typlen;
            else
                p += LONGALIGN(* (int32 *) p);
        /*
         * For the pair of double quotes
         */
        overall_length += 2;
        }
        overall_length += (strlen(values[i]) + 1);
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
}
/****************************************************************************/
char *
array_dims(v, isNull)
ArrayType *v;
bool *isNull;
{
    char *p, *save_p;
    int nbytes, i;
    int *dimv, *lb;

	if (v == (ArrayType *) NULL) RETURN_NULL;
    nbytes = ARR_NDIM(v)*25;
    save_p = p =  (char *) palloc(nbytes);
    dimv = ARR_DIMS(v); lb = ARR_LBOUND(v);
    for (i = 0; i < ARR_NDIM(v); i++) {
        sprintf(p, "[%d:%d]", lb[i], dimv[i]+lb[i]-1);
        p += strlen(p);
    }
    return (save_p);
} 
/************************************************************************/
/* This routing takes an array and an index array and returns a pointer
 * to the referred element */
Datum 
array_ref(array, n, indx, reftype, len, isNull)
ArrayType *array;
int indx[], n;
int reftype, len;
bool *isNull;
{
    int i, ndim, *dim, *lb, offset, nbytes;
    struct varlena *v;
	char *retval;

	if (array == (ArrayType *) NULL) RETURN_NULL;
    dim = ARR_DIMS(array);
	lb = ARR_LBOUND(array);
    ndim = ARR_NDIM(array);
	nbytes = (* (int32 *) array) - ARR_OVERHEAD(ndim);

    if (!SanityCheckInput(ndim, n,  dim, lb, indx))
        RETURN_NULL;

    GetOffset(offset, n, dim, lb, indx);

	if (ARR_IS_LO(array)) {
    char * lo_name;
    int fd;

    /* We are assuming fixed element lengths here */
	offset *= len;
    lo_name = (char *)ARR_DATA_PTR(array);
    if ((fd = LOopen(lo_name, O_RDONLY)) < 0)
        RETURN_NULL;

    if (LOlseek(fd, offset, L_SET) < 0)
        RETURN_NULL;

    v = (struct varlena *) LOread(fd, len);
    if (VARSIZE(v) - 4 < len)
        RETURN_NULL;
    (void) LOclose(fd);
	if (len < 0) return (Datum) v;
    return _ArrayCast((char *)VARDATA(v), reftype, len);
	}

	if (len >  0) {
		offset = offset * len;
		/*  off the end of the array */
    	if (nbytes - offset < 1) RETURN_NULL;
    	retval = ARR_DATA_PTR (array) + offset;
    	return _ArrayCast(retval, reftype, len);
	}

	else {
		bool done = false;
		char *temp;
		int bytes = nbytes;
		temp = ARR_DATA_PTR (array);
    	i = 0;
    	while (bytes > 0 && !done) {
        	if (i == offset) {
        	retval = temp;
        	done = true;
        	}
        	bytes -= LONGALIGN(* (int32 *) temp);
        	temp += LONGALIGN(* (int32 *) temp);
        	i++;
    	}

    	if (! done) RETURN_NULL;
    	return (Datum) retval;
	}
}

/************************************************************************/
/* This routine takes an array and a range of indices (upperIndex and 
 * lowerIndx), creates a new array structure for the referred elements 
 * and returns a pointer to it.
 */
Datum 
array_clip(array, n, upperIndx, lowerIndx, reftype, len, isNull)
ArrayType *array;
int upperIndx[], lowerIndx[], n;
int reftype, len;
bool *isNull;
{
    int i, ndim, *dim, *lb, offset, nbytes;
    struct varlena *v;
	char *retval;
	ArrayType *newArr; 
	int bytes, span[MAXDIM];

	if (array == (ArrayType *) NULL) RETURN_NULL;
    dim = ARR_DIMS(array);
	lb = ARR_LBOUND(array);
    ndim = ARR_NDIM(array);
	nbytes = (* (int32 *) array) - ARR_OVERHEAD(ndim);

    if (!SanityCheckInput(ndim, n,  dim, lb, upperIndx))
        RETURN_NULL;

	if (!SanityCheckInput(ndim, n, dim, lb, lowerIndx))
        RETURN_NULL;

	for (i = 0; i < n; i++)
		if (lowerIndx[i] > upperIndx[i])
		elog(WARN, "lowerIndex cannot be larger than upperIndx"); 
	get_range(n, span, lowerIndx, upperIndx);

	if (ARR_IS_LO(array)) {
    char * lo_name, * newname;
    int fd, newfd;

	if (len < 0) 
	elog(WARN, "array_clip: array of variable length objects not supported");  
    lo_name = (char *)ARR_DATA_PTR(array);
    if ((fd = LOopen(lo_name, O_RDONLY)) < 0)
        RETURN_NULL;
	newname = _array_newLO();
	_ReadLOArray(newname, &bytes, &newfd);
	bytes += ARR_OVERHEAD(n);
	newArr = (ArrayType *) palloc(bytes);
    bcopy(array, newArr, sizeof(ArrayType));
    bcopy(&bytes, newArr, sizeof(int));
    bcopy(span, ARR_DIMS(newArr), n*sizeof(int));
    bcopy(lowerIndx, ARR_LBOUND(newArr), n*sizeof(int));
    strcpy(ARR_DATA_PTR(newArr), newname);
	_LOArrayRange(lowerIndx, upperIndx, len, fd, newfd, array, 1, isNull);
    (void) LOclose(fd);
    (void) LOclose(newfd);
	if (*isNull) { pfree(newArr); newArr = NULL;}
    return ((Datum) newArr);
	}

	if (len >  0) {
		getNitems(bytes, n, span);
		bytes = bytes*len + ARR_OVERHEAD(n);
	}
	else {
		bytes = _ArrayClipCount(lowerIndx, upperIndx, array);
		bytes += ARR_OVERHEAD(n);
	}
	newArr = (ArrayType *) palloc(bytes);
	bcopy(array, newArr, sizeof(ArrayType));
	bcopy(&bytes, newArr, sizeof(int));
	bcopy(span, ARR_DIMS(newArr), n*sizeof(int));
	bcopy(lowerIndx, ARR_LBOUND(newArr), n*sizeof(int));
	_ArrayRange(lowerIndx, upperIndx, len, ARR_DATA_PTR(newArr), array, 1);
    return (Datum) newArr;
}

/********************************************************************/
char *
array_set(array, n, indx, dataPtr, reftype, len, isNull)
ArrayType *array;
int n, indx[];
bool *isNull;
char *dataPtr;
int reftype, len;
{
    int i, ndim, *dim, *lb, offset, nbytes;

	if (array == (ArrayType *) NULL) RETURN_NULL;
    dim = ARR_DIMS(array);
    lb = ARR_LBOUND(array);
    ndim = ARR_NDIM(array);
	nbytes = (* (int32 *) array) - ARR_OVERHEAD(ndim);

    if (!SanityCheckInput(ndim, n,  dim, lb, indx)) return((char *)array);
    GetOffset(offset, n, dim, lb, indx);

    if (ARR_IS_LO(array)) {
    int fd;
    char * lo_name;
    struct varlena *v;

    /* We are assuming fixed element lengths here */
    offset *= len;
    lo_name = ARR_DATA_PTR(array);
    if ((fd = LOopen(lo_name, O_WRONLY)) < 0)
		return((char *)array);

    if (LOlseek(fd, offset, L_SET) < 0)
		return((char *)array);
	v = (struct varlena *) palloc(len + 4);
	VARSIZE (v) = len + 4;
    ArrayCastAndSet(dataPtr, (bool) reftype, len, VARDATA(v));
    n =  LOwrite(fd, v);
    if (n < VARSIZE(v) - 4) RETURN_NULL;
    pfree(v);
    (void) LOclose(fd);
	return((char *)array);
    }
	else {
	char *pos;
    if (len >  0) {
        offset = offset * len;
        /*  off the end of the array */
        if (nbytes - offset < 1) return((char *)array);
        pos = ARR_DATA_PTR (array) + offset;
    }
    else {
    /*  bool done = false;
        char *temp;
        int bytes = nbytes;
        temp = ARR_DATA_PTR (array);
        i = 0;
        while (bytes > 0 && !done) {
            if (i == offset) {
            pos = temp;
            done = true;
            }
            bytes -= LONGALIGN(* (int32 *) temp);
            temp += LONGALIGN(* (int32 *) temp);
            i++;
        }
        if (! done) return((char *)array);
	*/
		elog(WARN, "array_set: update of variable length fields not supported");
	} 
    ArrayCastAndSet(dataPtr, (bool) reftype, len, pos);
    }
    return((char *)array);
}

/*--------------------------------------------------------------------*/
char * 
array_assgn(array, n, upperIndx, lowerIndx, newArr, reftype, len, isNull)
ArrayType *array, *newArr;
int upperIndx[], lowerIndx[], n;
int reftype, len;
bool *isNull;
{
    int i, ndim, *dim, *lb;
	
	if (array == (ArrayType *) NULL) RETURN_NULL;
	if (len < 0) 
	elog(WARN, "array_assgn: assignment on arrays of variable length elements not supported");  

    dim = ARR_DIMS(array);
	lb = ARR_LBOUND(array);
    ndim = ARR_NDIM(array);

    if (!SanityCheckInput(ndim, n,  dim, lb, upperIndx))
        RETURN_NULL;

	if (!SanityCheckInput(ndim, n, dim, lb, lowerIndx))
        RETURN_NULL;

	for (i = 0; i < n; i++)
		if (lowerIndx[i] > upperIndx[i])
		elog(WARN, "lowerIndex cannot be larger than upperIndx"); 

	if (ARR_IS_LO(array)) {
    char * lo_name;
    int fd, newfd;

    lo_name = (char *)ARR_DATA_PTR(array);
    if ((fd = LOopen(lo_name, O_WRONLY)) < 0)
        RETURN_NULL;
	if (ARR_IS_LO(newArr)) {
    lo_name = (char *)ARR_DATA_PTR(newArr);
    if ((newfd = LOopen(lo_name, O_RDONLY)) < 0)
        RETURN_NULL;
	_LOArrayRange(lowerIndx, upperIndx, len, fd, newfd, array, 0, 1, isNull);
    (void) LOclose(newfd);
	} else
	_LOArrayRange(lowerIndx, upperIndx, len, fd, ARR_DATA_PTR(newArr), 
		array, 0, 0, isNull);
    (void) LOclose(fd);
    return ((char *) array);
	}

	_ArrayRange(lowerIndx, upperIndx, len, ARR_DATA_PTR(newArr), array, 0);
    return (char *) array;
}
/***************************************************************************/
array_eq ( array1, array2 ) 
ArrayType *array1, *array2;
{
	if ( *(int *)array1 != *(int *)array2 ) return (0);
	if ( strncmp(array1, array2, *(int *)array1)) return(0);
	return(1);
}
/***************************************************************************/
/***************************UTILITIES***************************************/
/***************************************************************************/
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

/*****************************************************************************/
Datum
_ArrayCast(value, byval, len)
    char *value;
    bool byval;
    int len;
{
    if (byval) {
        switch (len) {
    case 1:
        return((Datum) * value);
    case 2:
        return((Datum) * (int16 *) value);
    case 3:
    case 4:
        return((Datum) * (int32 *) value);
    default:

        elog(WARN, "array_ref: byval and elt len > 4!");
            break;
        }
    } else {
        return (Datum) value;
	}
}

/******************************************************************************/
ArrayCastAndSet(src, typbyval, typlen, dest)
char *src, *dest;
int typlen;
bool typbyval;
{
	int inc;

    if (typlen > 0)
    {
      	if (typbyval) {
		  switch(typlen) {
	    	case 1: 
				*dest = DatumGetChar(src);
				break;
	    	case 2: 
				* (int16 *) dest = DatumGetInt16(src);
				break;
	    	case 4:
				* (int32 *) dest = (int32)src;
				break;
			  }
        }
        else bcopy(src, dest, typlen);
        inc = typlen;
    }
    else
    {
        bcopy(src, dest, * (int32 *) src);
       inc = (LONGALIGN(* (int32 *) src));
    }
	return(inc);
} 
/************************************************************************/
SanityCheckInput(ndim, n, dim, lb, indx)
int ndim, n, dim[], lb[], indx[];
{
    int i;
    /* Do Sanity check on input */
    if (n != ndim) elog(WARN, "array dimension does not match");
    for (i = 0; i < ndim; i++)
        if ((lb[i] > indx[i]) || (indx[i] >= (dim[i] + lb[i])))
            return(0);
	return(1);
}
/***********************************************************************/
_ArrayRange(st, endp, bsize,  destPtr, array, from)
int st[], endp[], bsize, from;
char * destPtr;
ArrayType *array;
{
    int n, *dim, *lb, st_pos, prod[MAXDIM];
    int span[MAXDIM], dist[MAXDIM], indx[MAXDIM];
    int i, j, inc;
	char *srcPtr;

    n = ARR_NDIM(array); dim = ARR_DIMS(array);
	lb = ARR_LBOUND(array); srcPtr = ARR_DATA_PTR(array);
	for (i = 0; i < n; st[i] -= lb[i], endp[i] -= lb[i], i++); 
    get_prod(n, dim, prod);
    tuple2linear(n, st_pos, st, prod);
	srcPtr = array_seek(srcPtr, bsize, st_pos);
    get_range(n, span, st, endp);
    get_offset_values(n, dist, prod, span);
    for (i=0; i < n; indx[i++]=0);
    i = j = n-1; inc = bsize;
    do {
        srcPtr = array_seek(srcPtr, bsize,  dist[j]); 
		if (from) inc = array_read(destPtr, bsize, 1, srcPtr);
		else inc = array_read(srcPtr, bsize, 1, destPtr);
        destPtr += inc; srcPtr += inc;
    } while ((j = next_tuple(i+1, indx, span)) != -1);
}

/***********************************************************************/
_ArrayClipCount(stI, endpI, array)
int stI[], endpI[];
ArrayType *array;
{
    int n, *dim, *lb, st_pos, prod[MAXDIM];
    int span[MAXDIM], dist[MAXDIM], indx[MAXDIM];
    int i, j, inc, st[MAXDIM], endp[MAXDIM];
	int count = 0;
	char *ptr;

    n = ARR_NDIM(array); dim = ARR_DIMS(array);
    lb = ARR_LBOUND(array); ptr = ARR_DATA_PTR(array);
    for (i = 0; i < n; st[i] = stI[i]-lb[i], endp[i]=endpI[i]-lb[i], i++);
    get_prod(n, dim, prod);
    tuple2linear(n, st_pos, st, prod);
	ptr = array_seek(ptr, -1, st_pos);
    get_range(n, span, st, endp);
    get_offset_values(n, dist, prod, span);
    for (i=0; i < n; indx[i++]=0);
    i = j = n-1;
    do {
        ptr = array_seek(ptr, -1,  dist[j]);
        inc =  LONGALIGN(* (int *) ptr);
        ptr += inc; count += inc;
    } while ((j = next_tuple(i+1, indx, span)) != -1);
	return(count);
}

/***********************************************************************/
char *
array_seek(ptr, eltsize, nitems)
int eltsize, nitems;
char *ptr;
{
	int i;

	if (eltsize > 0) return(ptr + eltsize*nitems);
	for (i = 0; i < nitems; i++) 
      	ptr += LONGALIGN(* (int *) ptr);
	return(ptr);
}
/***********************************************************************/
array_read(destptr, eltsize, nitems, srcptr)
int eltsize, nitems;
char *srcptr, *destptr;
{
	int i, inc, tmp;

	if (eltsize > 0)  {
		bcopy(srcptr, destptr, eltsize*nitems);
		return(eltsize*nitems);
	}
	for (i = inc = 0; i < nitems; i++) {
      tmp = (LONGALIGN(* (int *) srcptr));
	  bcopy(srcptr, destptr, tmp);
      srcptr += tmp;
      destptr += tmp;
	  inc += tmp;
	}
	return(inc);
}
/***********************************************************************/
_LOArrayRange(st, endp, bsize, srcfd, destfd, array, from, isSrcLO, isNull)
int st[], endp[], bsize, from, isSrcLO;
int srcfd, destfd;
ArrayType *array;
bool *isNull;
{
    int n, *dim, st_pos, prod[MAXDIM];
    int span[MAXDIM], dist[MAXDIM], indx[MAXDIM];
    int i, j, inc, tmp, *lb, offset;

    n = ARR_NDIM(array); dim = ARR_DIMS(array);
    lb = ARR_LBOUND(array);
    for (i = 0; i < n; st[i] -= lb[i], endp[i] -= lb[i], i++);

    get_prod(n, dim, prod);
    tuple2linear(n, st_pos, st, prod);
	offset = st_pos*bsize;
	if (LOlseek(srcfd, offset, L_SET) < 0) RETURN_NULL;
    get_range(n, span, st, endp);
    get_offset_values(n, dist, prod, span);
    for (i=0; i < n; indx[i++]=0);
	for (i = n-1, inc = bsize; i >= 0; inc *= span[i--])
        if (dist[i]) break;
    j = n-1;
    do {
		offset += (dist[j]*bsize);
		if (LOlseek(srcfd,  offset, L_SET) < 0) RETURN_NULL;
		if (from) tmp = _LOtransfer(&destfd, inc, 1, &srcfd, isSrcLO);
		else tmp = _LOtransfer(&srcfd, inc, 1, &destfd, isSrcLO);
		if ( tmp < inc ) RETURN_NULL;
		offset += inc;
    } while ((j = next_tuple(i+1, indx, span)) != -1);
}

/***********************************************************************/
_LOtransfer(destfd, size, nitems, srcfd, isSrcLO)
int *destfd, *srcfd, size, nitems, isSrcLO;
{
	struct varlena *v;
	int tmp, inc;
	char buf[8000];

	inc = nitems*size; 
	if (isSrcLO) { 
		v = (struct varlena *) LOread(*srcfd, inc);
		if (VARSIZE(v) - 4 < inc) return(-1);
	} else {
		v = (struct varlena *)buf;
		VARSIZE(v) = inc + sizeof(int);
		bcopy (*srcfd, VARDATA(v), inc); 	
		*srcfd += inc;
	}
	tmp = LOwrite(*destfd, v);	
	return(tmp);
}

/***********************************************************************/
char *
_array_set(array, indx_str, dataPtr)
ArrayType *array;
struct varlena *indx_str, *dataPtr;
{
    int len, i;
    struct varlena *v;
    int indx[MAXDIM];
    char *retval, *s, *comma, *r;
    bool isNull;

    /* convert index string to index array */
    s = VARDATA(indx_str);
    for (i=0; *s != '\0'; i++) {
        while (isSpace(*s))
            s++;
        r = s;
        if ((comma = (char *)index(s, ',')) != (char *) NULL) {
            *comma = '\0';
            s = comma + 1;
        } else while (*s != '\0') s++;
        if ((indx[i] = atoi(r)) < 0)
            elog(WARN, "array dimensions must be non-negative");
    }
	retval = array_set(array, i, indx, dataPtr, &isNull);
    pfree(v);
    return(retval);
}

/***************************************************************************/
/*-----------------------------------------------------------------------------
 generates the tuple that is lexicographically one greater than the current
   n-tuple in "curr", with the restriction that the i-th element of "curr" is
   less than the i-th element of "span".
   RETURNS   0   if no next tuple exists
             1   otherwise
-----------------------------------------------------------------------------*/
next_tuple(n, curr,  span)
int n, span[], curr[];
{
    int i;

	if (!n) return(-1);
    curr[n-1] = (curr[n-1]+1)%span[n-1];
    for (i = n-1; i*(!curr[i]); i--)
        curr[i-1] = (curr[i-1]+1)%span[i-1];

    if (i) return(i);
    if (curr[0]) return(0);
    return(-1);
}
/**********************************************************************/
char *
_array_newLO()
{
    char *p;
    p = (char *) palloc(30);
    sprintf(p, "/ArryClip.%d", newoid());
    return (p);
}
/**********************************************************************/
