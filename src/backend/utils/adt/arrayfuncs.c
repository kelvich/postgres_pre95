 /*
 * arrayfuncs.c --
 *     Special functions for arrays.
 *
 *
 * RcsId("$Header$");
 */

#include <ctype.h>
#include <stdio.h>

#include "tmp/postgres.h"
#include "tmp/libpq-fs.h"

#include "catalog/pg_type.h"
#include "catalog/syscache.h"
#include "catalog/pg_lobj.h"

#include "utils/palloc.h"
#include "utils/memutils.h"
#include "storage/fd.h"		/* for SEEK_ */
#include "fmgr.h"
#include "utils/log.h"
#include "array.h"

#define ASSGN    "="

/* An array has the following internal structure:
 *    <nbytes>            - total number of bytes 
 *     <ndim>                - number of dimensions of the array 
 *    <flags>                - bit mask of flags
 *    <dim>                - size of each array axis 
 *    <dim_lower>            - lower boundary of each dimension 
 *    <actual data>        - whatever is the stored data
*/

/*-=-=--=-=-=-=-=-=-=-=--=-=-=-=-=-=-=-=--=-=-=-=-=-=-=-=--=-=-=-=-=-=-=-*/
char *_array_newLO(), *_ChunkArray(), * array_dims();
struct varlena * _ReadChunkArray1El();
Datum _ArrayCast();

/*---------------------------------------------------------------------
 * array_in : 
 *        converts an array from the external format in "string" to
 *        it internal format.
 * return value :
 *        the internal representation of the input array
 *--------------------------------------------------------------------
 */
char *
array_in(string, element_type)
char *string;                    /* input array in external form */
ObjectId element_type;            /* type OID of an array element */

{
    int typlen;
    bool typbyval, done;
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
    char *_ReadLOArray(), *_ReadArrayStr();

    system_cache_lookup(element_type, true, &typlen, &typbyval, &typdelim, 
                &typelem, &typinput);

    fmgr_info(typinput, &inputproc, &dummy);

    string_save = (char *) palloc(strlen(string) + 3);
    strcpy(string_save, string);

    /* --- read array dimensions  ---------- */
    p = q = string_save; done = false;
    for ( ndim = 0; !done; ) {
        while (isspace(*p)) p++;
        if (*p == '[' ) {
                p++;
                if ((r = (char *)index(p, ':')) == (char *)NULL)
                    lBound[ndim] = 1;
                else { 
                    *r = '\0'; 
                    lBound[ndim] = atoi(p); 
                    p = r + 1;
                }
                for (q = p; isdigit(*q); q++);
                if (*q != ']')
                  elog(WARN, "array_in: missing ']' in array declaration");
                *q = '\0';
                dim[ndim] = atoi(p);
                if ((dim[ndim] < 0) || (lBound[ndim] < 0))
                    elog(WARN,"array_in: array dimensions need to be positive");
                dim[ndim] = dim[ndim] - lBound[ndim] + 1;
                if (dim[ndim] < 0)
                    elog(WARN, "array_in: upper_bound cannot be < lower_bound");
                p = q + 1; ndim++;
        } else {
            done = true;
        }
    }

    if (ndim == 0) {
        if (*p == '{') {
            ndim = _ArrayCount(p, dim, typdelim);
            for (i = 0; i < ndim; lBound[i++] = 1);
        } else { 
             elog(WARN,"array_in: Need to specify dimension");
        }
    } else {
        while (isspace(*p)) p++;
        if (strncmp(p, ASSGN, strlen(ASSGN)))
            elog(WARN, "array_in: missing assignment operator");
        p+= strlen(ASSGN);
        while (isspace(*p)) p++;
    }        

    nitems = getNitems( ndim, dim);
    if (nitems == 0) {
        char *emptyArray = palloc(sizeof(ArrayType));
        bzero(emptyArray, sizeof(ArrayType));
        * (int32 *) emptyArray = sizeof(ArrayType);
        return emptyArray;
    }

    if (*p == '{') {
        /* array not a large object */
        dataPtr = (char *) _ReadArrayStr(p, nitems,  ndim, dim, inputproc, typelem,
                    typdelim, typlen, typbyval, &nbytes );
        nbytes += ARR_OVERHEAD(ndim);
        retval = (ArrayType *) palloc(nbytes);
        bzero (retval, nbytes);
        bcopy(&nbytes, retval, sizeof(int));
        bcopy(&ndim, ARR_NDIM_PTR(retval), sizeof(int));
        SET_LO_FLAG (false, retval);
        bcopy(dim, ARR_DIMS(retval), ndim*sizeof(int));
        bcopy(lBound, ARR_LBOUND(retval), ndim*sizeof(int));
        _CopyArrayEls(dataPtr, ARR_DATA_PTR(retval), nitems, typlen, typbyval);
    } else  {
        int dummy, bytes, lobjtype;
        bool chunked = false;

        dataPtr = _ReadLOArray(p, &bytes, &dummy, &chunked, &lobjtype, ndim, 
                dim, typlen );
        nbytes = bytes + ARR_OVERHEAD(ndim);
        retval = (ArrayType *) palloc(nbytes);
        bzero (retval, nbytes);
        bcopy(&nbytes, retval, sizeof(int));
        bcopy(&ndim, ARR_NDIM_PTR(retval), sizeof(int));
        SET_LO_FLAG (true, retval);
        SET_CHUNK_FLAG (chunked, retval);
        SET_OBJ_TYPE (lobjtype, retval);
        bcopy(dim, ARR_DIMS(retval), ndim*sizeof(int));
        bcopy(lBound, ARR_LBOUND(retval), ndim*sizeof(int));
        bcopy(dataPtr, ARR_DATA_PTR(retval), bytes);
    }
    pfree(string_save);
    return((char *)retval);
}

/*-------------------------------------------------------------------------------
 * _ArrayCount --
 *   Counts the number of dimensions and the dim[] array for an array string. 
 *	 The syntax for array input is C-like nested curly braces 
 *--------------------------------------------------------------------------------
 */
int 
_ArrayCount(str, dim, typdelim)
int dim[], typdelim;
char *str;
{
    int nest_level = 0, i; 
    int ndim = 0, temp[MAXDIM];
    bool scanning_string = false;
    bool eoArray = false;
    char *q;

    for (i = 0; i < MAXDIM; ++i) {
	temp[i] = dim[i] = 0;
    }

    if (strncmp (str, "{}", 2) == 0) return(0); 

    q = str;
    while (eoArray != true) {
        bool done = false;
        while (!done) {
            switch (*q) {
                case '\"':
                     scanning_string = ! scanning_string;
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
        if (!eoArray) 
            while (isspace(*q)) q++;
    }
    for (i = 0; i < ndim; ++i) {
	dim[i] = temp[i];
    }
    
    return(ndim);
}

/*------------------------------------------------------------------------------
 * _ReadArrayStr :
 *   parses the array string pointed by "arrayStr" and converts it in the 
 *   internal format. The external format expected is like C array declaration. 
 *   Unspecified elements are initialized to zero for fixed length base types
 *   and to empty varlena structures for variable length base types.
 * result :
 *   returns the internal representation of the array elements
 *   nbytes is set to the size of the array in its internal representation.
 *----------------------------------------------------------------------------*/
char *
_ReadArrayStr(arrayStr, nitems, ndim, dim, inputproc, typelem, typdelim, typlen, typbyval, nbytes)
int nitems, ndim, dim[], typlen, *nbytes;
ObjectId typelem;
char typdelim, *arrayStr;
func_ptr inputproc;                     /* function used for the conversion */
bool typbyval;
{
    int i, dummy, nest_level = 0;
    char *p, *q, *r, **values;
    bool scanning_string = false;
    int indx[MAXDIM], prod[MAXDIM];
    bool eoArray = false;

    get_prod(ndim, dim, prod);
    for (i = 0; i < ndim; indx[i++] = 0);
    /* read array enclosed within {} */    
    values = (char **) palloc(nitems * sizeof(char *));
    bzero(values, nitems * sizeof(char *));
    q = p = arrayStr;

    while ( ! eoArray ) {
        bool done = false;
        int i = -1;

        while (!done) {
            switch (*q) {
                case '\\':
                    /* Crunch the string on top of the backslash. */
                    for (r = q; *r != '\0'; r++) *r = *(r+1);
                    break;
                case '\"':
                    if (!scanning_string ) {
                        while (p != q) p++;
                        p++; /* get p past first doublequote */
                    } else 
                        *q = '\0';
                    scanning_string = ! scanning_string;
                    break;
                case '{':
                    if (!scanning_string) { 
                        p++; 
                        nest_level++;
                        if (nest_level > ndim)
                            elog(WARN, "array_in: illformed array constant");
                        indx[nest_level - 1] = 0;
                        indx[ndim - 1] = 0;
                    }
                    break;
                case '}':
                    if (!scanning_string) {
                        if (i == -1)
                            i = tuple2linear(ndim, indx, prod); 
                        nest_level--;
                        if (nest_level == 0)
                            eoArray = done = true;
                        else { 
                            *q = '\0';
                            indx[nest_level - 1]++;
                        }
                    }
                    break;
                default:
                    if (*q == typdelim && !scanning_string ) {
                        if (i == -1) 
                            i = tuple2linear(ndim, indx, prod); 
                        done = true;
                        indx[ndim - 1]++;
                    }
                    break;
            }
            if (!done) 
                q++;
        }
        *q = '\0';                    
        if (i >= nitems)
            elog(WARN, "array_in: illformed array constant");
        values[i] = (*inputproc) (p, typelem);
        p = ++q;
        if (!eoArray)    
            /* 
             * if not at the end of the array skip white space 
             */
            while (isspace(*q)) {
                p++;
                q++;
            }
    }
    if (typlen > 0) {
        *nbytes = nitems * typlen;
        if (!typbyval)
            for (i = 0; i < nitems; i++)
                if (!values[i]) {
                    values[i] = palloc(typlen);
                    bzero(values[i], typlen);
                }
    } else {
        for (i = 0, *nbytes = 0; i < nitems; i++) {
            if (values[i])
                *nbytes += INTALIGN(* (int32 *) values[i]);
            else {
                *nbytes += sizeof(int32);
                values[i] = palloc(sizeof(int32));
                *(int32 *)values[i] = sizeof(int32); 
            }
        }
    }
    return((char *)values);
}


/*---------------------------------------------------------------------------------
 * Read data about an array to be stored as a large object                    
 *--------------------------------------------------------------------------------
 */
char *
_ReadLOArray(str, nbytes, fd, chunkFlag, lobjtype, ndim, dim, baseSize )
char *str;
int *nbytes, *fd, *lobjtype;
bool *chunkFlag;
int ndim, dim[], baseSize;
{
    char *inputfile, *accessfile = NULL, *chunkfile = NULL;
    char *retStr, *_AdvanceBy1word();

    *lobjtype = Unix;	/* default */

    str = _AdvanceBy1word(str, &inputfile); 

    while (str != NULL) {
        char *space, *word;
        
        str = _AdvanceBy1word(str, &word); 

        if (!strcmp (word, "-chunk")) {
            if (str == NULL)
                elog(WARN, "array_in: access pattern file required");
            str = _AdvanceBy1word(str, &accessfile);
        }
        else if (!strcmp (word, "-noreorg"))  {
            if (str == NULL)
                elog(WARN, "array_in: chunk file required");
            str = _AdvanceBy1word(str, &chunkfile);
        }

        else if (!strcmp(word, "-invert"))
            *lobjtype = Inversion;
        else if (!strcmp(word, "-unix"))
            *lobjtype = Unix;
        else if (!strcmp(word, "-external"))
            *lobjtype = External;
        else if (!strcmp(word, "-jaquith"))
            *lobjtype = Jaquith;
        else {
	    *lobjtype = Unix;
            elog(WARN, "usage: <input file> -chunk DEFAULT/<access pattern file> -invert/-native [-noreorg <chunk file>]");
	}
    }

    if (inputfile == NULL) 
        elog(WARN, "array_in: missing file name"); 
    *fd = -1;
    if (FilenameToOID(inputfile) == InvalidObjectId) {
        char * saveName = palloc(strlen(inputfile) + 2);
        strcpy (saveName, inputfile);
        if (*lobjtype == Inversion)
            *fd = LOcreat (saveName, INV_READ, *lobjtype);
        else 
            *fd = LOcreat (saveName, 0600, *lobjtype);
        if ( *fd < 0 )
               elog(WARN, "Large object create failed");
        pfree(saveName);
    } 
    retStr = inputfile;
    *nbytes = strlen(retStr) + 2;

    if ( accessfile ) {
        FILE *afd;
        if ((afd = fopen (accessfile, "r")) == NULL)
            elog(WARN, "unable to open access pattern file");
        *chunkFlag = true;
        if (*fd == -1) 
            if ((*fd = LOopen(inputfile,
			      (*lobjtype == Inversion)?INV_READ:O_RDONLY)) < 0)
                elog(WARN, "array_in:Unable to open input file");
        retStr = _ChunkArray(*fd, afd, ndim, dim, baseSize, nbytes, *lobjtype, chunkfile);
    }    
    return(retStr);
}
/*-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-*/
_CopyArrayEls( values, p, nitems, typlen, typbyval)
bool typbyval;
int nitems, typlen;
char *p, **values;
{
    int i;

    for (i = 0; i < nitems; i++) {
        int inc;
        inc = ArrayCastAndSet((char *)values[i], typbyval, typlen, p);
        p += inc;
        if (!typbyval) 
            pfree((char *)values[i]);
    }
    pfree(values);
}

/*-------------------------------------------------------------------------------------*/
/* array_out : 
 *         takes the internal representation of an array and returns a string
 *        containing the array in its external format.
 *------------------------------------------------------------------------------------
 */
char *
array_out(v, element_type)
ArrayType *v;
ObjectId element_type;
{
    int typlen;
    bool typbyval;
    char typdelim;
    ObjectId typoutput, typelem;
    func_ptr outputproc;

    char *p, *q, *retval, **values, delim[2];
    int nitems, nbytes, overall_length, i, j, k, dummy, indx[MAXDIM];
    int ndim, *dim, *lBound;

    if (v == (ArrayType *) NULL)
        return ((char *) NULL);

    if (ARR_IS_LO(v) == true)  {
        char *p, *save_p;
        int nbytes, i;

        /* get a wide string to print to */
        p = array_dims(v, &dummy);
        nbytes = strlen(ARR_DATA_PTR(v)) + 4 + *(int *)p;

        save_p = (char *) palloc(nbytes);

        strcpy(save_p, p + sizeof(int));
        strcat(save_p, ASSGN);
        strcat(save_p, ARR_DATA_PTR(v));
        pfree(p);
        return (save_p);
    }

    system_cache_lookup(element_type, false, &typlen, &typbyval,
                        &typdelim, &typelem, &typoutput);
    fmgr_info(typoutput, & outputproc, &dummy);
    sprintf(delim, "%c", typdelim);
    ndim = ARR_NDIM(v);
    dim = ARR_DIMS(v);
    nitems = getNitems(ndim, dim);

    if (nitems == 0) {
        char *emptyArray = palloc(3);
        emptyArray[0] = '{';
        emptyArray[1] = '}';
        emptyArray[2] = '\0';
        return emptyArray;
      }

    p = ARR_DATA_PTR(v);
    overall_length = 0;
    values = (char **) palloc(nitems * sizeof (char *));
    for (i = 0; i < nitems; i++) {
        if (typbyval) {
            switch(typlen) {
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
        } else {
            values[i] = (*outputproc) (p, typelem);
            if (typlen > 0)
                p += typlen;
            else
                p += INTALIGN(* (int32 *) p);
            /*
              * For the pair of double quotes
              */
            overall_length += 2;
        }
        overall_length += (strlen(values[i]) + 1);
    }

    /* 
     * count total number of curly braces in output string 
     */
    for (i = j = 0, k = 1; i < ndim; k *= dim[i++], j += k);

    p = (char *) palloc(overall_length + 2*j);        
    retval = p;

    strcpy(p, "{");
    for (i = 0; i < ndim; indx[i++] = 0);
    j = 0; k = 0;
    do {
        for (i = j; i < ndim - 1; i++)
            strcat(p, "{");
        /*
        * Surround anything that is not passed by value in double quotes.
        * See above for more details.
        */
        if (!typbyval) {
               strcat(p, "\"");
               strcat(p, values[k]);
               strcat(p, "\"");
        } else
               strcat(p, values[k]);
        pfree(values[k++]); 

        for (i = ndim - 1; i >= 0; i--) {
            indx[i] = (indx[i] + 1)%dim[i];
            if (indx[i]) {
                strcat (p, delim);
                break;
            } else  
                strcat (p, "}");
        }
        j = i;
    } while (j  != -1);

    pfree((char *)values);
    return(retval);
}
/*---------------------------------------------------------------------------------
 * array_dims :
 *        returns the dimension of the array pointed to by "v"
 *--------------------------------------------------------------------------------- 
 */
char *
array_dims(v, isNull)
ArrayType *v;
bool *isNull;
{
    char *p, *save_p;
    int nbytes, i;
    int *dimv, *lb;

    if (v == (ArrayType *) NULL) RETURN_NULL;
    nbytes = ARR_NDIM(v)*33;    
    /* 
     * 33 since we assume 15 digits per number + ':' +'[]' 
     */         
    save_p = p =  (char *) palloc(nbytes + 4);
    bzero(save_p, nbytes + 4);
    dimv = ARR_DIMS(v); lb = ARR_LBOUND(v);
    p += 4;
    for (i = 0; i < ARR_NDIM(v); i++) {
        sprintf(p, "[%d:%d]", lb[i], dimv[i]+lb[i]-1);
        p += strlen(p);
    }
    nbytes = strlen(save_p + 4) + 4;
    bcopy(&nbytes, save_p, 4);
    return (save_p);
} 

/*---------------------------------------------------------------------------
 * array_ref :
 *         This routing takes an array pointer and an index array and returns a pointer
 *         to the referred element if element is passed by reference otherwise 
 *        returns the value of the referred element.
 *---------------------------------------------------------------------------
 */
Datum 
array_ref(array, n, indx, reftype, elmlen, arraylen, isNull)
ArrayType *array;
int indx[], n;
int reftype, elmlen, arraylen;
bool *isNull;
{
    int i, ndim, *dim, *lb, offset, nbytes;
    struct varlena *v;
    char *retval;

    if (array == (ArrayType *) NULL) RETURN_NULL;
    if (arraylen > 0) {
        /*
         * fixed length arrays -- these are assumed to be 1-d
         */
        if (indx[0]*elmlen > arraylen) 
            elog(WARN, "array_ref: array bound exceeded");
        retval = (char *)array + indx[0]*elmlen;
        return _ArrayCast(retval, reftype, elmlen);
    }
    dim = ARR_DIMS(array);
    lb = ARR_LBOUND(array);
    ndim = ARR_NDIM(array);
    nbytes = (* (int32 *) array) - ARR_OVERHEAD(ndim);

    if (!SanityCheckInput(ndim, n,  dim, lb, indx))
        RETURN_NULL;

    offset = GetOffset(n, dim, lb, indx);

    if (ARR_IS_LO(array)) {
        char * lo_name;
        int fd;

        /* We are assuming fixed element lengths here */
        offset *= elmlen;
        lo_name = (char *)ARR_DATA_PTR(array);
        if ((fd = LOopen(lo_name, ARR_IS_INV(array)?INV_READ:O_RDONLY)) < 0)
            RETURN_NULL;

        if (ARR_IS_CHUNKED(array))
            v = _ReadChunkArray1El(indx, elmlen, fd, array, isNull);
        else {
            if (LOlseek(fd, offset, SEEK_SET) < 0)
                RETURN_NULL;
            v = (struct varlena *) LOread(fd, elmlen);
        }
        if (*isNull) RETURN_NULL;
        if (VARSIZE(v) - 4 < elmlen)
            RETURN_NULL;
        (void) LOclose(fd);
        retval  = (char *)_ArrayCast((char *)VARDATA(v), reftype, elmlen);
	if ( reftype == 0) { /* not by value */
	    char * tempdata = palloc (elmlen);
	    bcopy (retval, tempdata, elmlen);
	    retval = tempdata;
	}
	pfree(v);
        return (Datum) retval;
    }
    
    if (elmlen >  0) {
        offset = offset * elmlen;
        /*  off the end of the array */
        if (nbytes - offset < 1) RETURN_NULL;
        retval = ARR_DATA_PTR (array) + offset;
        return _ArrayCast(retval, reftype, elmlen);
    } else {
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
            bytes -= INTALIGN(* (int32 *) temp);
            temp += INTALIGN(* (int32 *) temp);
            i++;
        }
        if (! done) 
            RETURN_NULL;
        return (Datum) retval;
    }
}

/*-------------------------------------------------------------------------------
 * array_clip :
 *        This routine takes an array and a range of indices (upperIndex and 
 *         lowerIndx), creates a new array structure for the referred elements 
 *         and returns a pointer to it.
 *--------------------------------------------------------------------------------
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

    /* timer_start(); */
    if (array == (ArrayType *) NULL) 
        RETURN_NULL;
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
        int fd, newfd, isDestLO = true, rsize;

        if (len < 0) 
            elog(WARN, "array_clip: array of variable length objects not supported");  
        lo_name = (char *)ARR_DATA_PTR(array);
        if ((fd = LOopen(lo_name, ARR_IS_INV(array)?INV_READ:O_RDONLY)) < 0)
            RETURN_NULL;
        newname = _array_newLO( &newfd, Unix );
        bytes = strlen(newname) + 1 + ARR_OVERHEAD(n);
        newArr = (ArrayType *) palloc(bytes);
        bcopy(array, newArr, sizeof(ArrayType));
        bcopy(&bytes, newArr, sizeof(int));
        bcopy(span, ARR_DIMS(newArr), n*sizeof(int));
        bcopy(lowerIndx, ARR_LBOUND(newArr), n*sizeof(int));
        strcpy(ARR_DATA_PTR(newArr), newname);

        rsize = compute_size (lowerIndx, upperIndx, n, len);
        if (rsize < MAX_BUFF_SIZE) { 
            char *buff;
            rsize += 4;
            buff = palloc (rsize);
            if ( buff ) 
                isDestLO = false;
            if (ARR_IS_CHUNKED(array)) {
                  _ReadChunkArray(lowerIndx, upperIndx, len, fd, &(buff[4]), 
                                                            array,0,isNull);
            } else { 
                  _ReadArray(lowerIndx, upperIndx, len, fd, &(buff[4]),array, 
                            0,isNull);
            }
            bcopy (&rsize, buff, 4);
            if (! *isNull)
                bytes = LOwrite ( newfd, buff );
            pfree (buff);
        }
        if (isDestLO)   
            if (ARR_IS_CHUNKED(array)) {
                  _ReadChunkArray(lowerIndx, upperIndx, len, fd, newfd, array,
                            1,isNull);
            } else {
                  _ReadArray(lowerIndx, upperIndx, len, fd, newfd, array, 1,isNull);
            }
        (void) LOclose(fd);
        (void) LOclose(newfd);
        if (*isNull) { 
            pfree(newArr); 
            newArr = NULL;
        }
        /* timer_end(); */
        return ((Datum) newArr);
    }

    if (len >  0) {
        bytes = getNitems(n, span);
        bytes = bytes*len + ARR_OVERHEAD(n);
    } else {
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

/*-------------------------------------------------------------------------------
 * array_set :
 *        This routine sets the value of an array location (specified by an index array)
 *        to a new value specified by "dataPtr".
 * result :
 *        returns a pointer to the modified array.
 *--------------------------------------------------------------------------------
 */
char *
array_set(array, n, indx, dataPtr, reftype, elmlen, arraylen, isNull)
ArrayType *array;
int n, indx[];
bool *isNull;
char *dataPtr;
int reftype, elmlen, arraylen;
{
    int i, ndim, *dim, *lb, offset, nbytes;
    char *pos;

    if (array == (ArrayType *) NULL) 
        RETURN_NULL;
    if (arraylen > 0) {
        /*
         * fixed length arrays -- these are assumed to be 1-d
         */
        if (indx[0]*elmlen > arraylen) 
            elog(WARN, "array_ref: array bound exceeded");
        pos = (char *)array + indx[0]*elmlen;
        ArrayCastAndSet(dataPtr, (bool) reftype, elmlen, pos);
        return((char *)array);
    }
    dim = ARR_DIMS(array);
    lb = ARR_LBOUND(array);
    ndim = ARR_NDIM(array);
    nbytes = (* (int32 *) array) - ARR_OVERHEAD(ndim);

    if (!SanityCheckInput(ndim, n,  dim, lb, indx)) 
        return((char *)array);
    offset = GetOffset( n, dim, lb, indx);

    if (ARR_IS_LO(array)) {
        int fd;
        char * lo_name;
        struct varlena *v;

        /* We are assuming fixed element lengths here */
        offset *= elmlen;
        lo_name = ARR_DATA_PTR(array);
        if ((fd = LOopen(lo_name, ARR_IS_INV(array)?INV_WRITE:O_WRONLY)) < 0)
            return((char *)array);

        if (LOlseek(fd, offset, SEEK_SET) < 0)
            return((char *)array);
        v = (struct varlena *) palloc(elmlen + 4);
        VARSIZE (v) = elmlen + 4;
        ArrayCastAndSet(dataPtr, (bool) reftype, elmlen, VARDATA(v));
        n =  LOwrite(fd, v);
        /* if (n < VARSIZE(v) - 4) 
            RETURN_NULL;
	*/
        pfree(v);
        (void) LOclose(fd);
        return((char *)array);
    }
    if (elmlen >  0) {
        offset = offset * elmlen;
        /*  off the end of the array */
        if (nbytes - offset < 1) return((char *)array);
        pos = ARR_DATA_PTR (array) + offset;
    } else {
        elog(WARN, "array_set: update of variable length fields not supported");
    } 
    ArrayCastAndSet(dataPtr, (bool) reftype, elmlen, pos);
    return((char *)array);
}

/*-------------------------------------------------------------------------------
 * array_assgn :
 *        This routine sets the value of a range of array locations (specified by upper
 *        and lower index values ) to new values passed as another array
 * result :
 *        returns a pointer to the modified array.
 *--------------------------------------------------------------------------------
 */
char * 
array_assgn(array, n, upperIndx, lowerIndx, newArr, reftype, len, isNull)
ArrayType *array, *newArr;
int upperIndx[], lowerIndx[], n;
int reftype, len;
bool *isNull;
{
    int i, ndim, *dim, *lb;
    
    if (array == (ArrayType *) NULL) 
        RETURN_NULL;
    if (len < 0) 
        elog(WARN,"array_assgn:updates on arrays of variable length elements not allowed");

    dim = ARR_DIMS(array);
    lb = ARR_LBOUND(array);
    ndim = ARR_NDIM(array);

    if (!SanityCheckInput(ndim, n,  dim, lb, upperIndx) || 
            !SanityCheckInput(ndim, n, dim, lb, lowerIndx)) {
        return((char *)array);
    }

    for (i = 0; i < n; i++)
        if (lowerIndx[i] > upperIndx[i])
            elog(WARN, "lowerIndex larger than upperIndx"); 

    if (ARR_IS_LO(array)) {
        char * lo_name;
        int fd, newfd;

        lo_name = (char *)ARR_DATA_PTR(array);
        if ((fd = LOopen(lo_name,  ARR_IS_INV(array)?INV_WRITE:O_WRONLY)) < 0)
            return((char *)array);
        if (ARR_IS_LO(newArr)) {
            lo_name = (char *)ARR_DATA_PTR(newArr);
            if ((newfd = LOopen(lo_name, ARR_IS_INV(newArr)?INV_READ:O_RDONLY)) < 0)
                return((char *)array);
            _LOArrayRange(lowerIndx, upperIndx, len, fd, newfd, array, 1, isNull);
            (void) LOclose(newfd);
        } else {
            _LOArrayRange(lowerIndx, upperIndx, len, fd, ARR_DATA_PTR(newArr), 
                        array, 0, isNull);
        }
        (void) LOclose(fd);
        return ((char *) array);
    }
    _ArrayRange(lowerIndx, upperIndx, len, ARR_DATA_PTR(newArr), array, 0);
    return (char *) array;
}
/*-------------------------------------------------------------------------------
 * array_eq :
 *        compares two arrays for equality    
 * result :
 *        returns 1 if the arrays are equal, 0 otherwise.
 *--------------------------------------------------------------------------------
 */
array_eq ( array1, array2 ) 
ArrayType *array1, *array2;
{
    if ((array1 == NULL) || (array2 == NULL)) return(0);
    if ( *(int *)array1 != *(int *)array2 ) return (0);
    if ( bcmp(array1, array2, *(int *)array1)) return(0);
    return(1);
}
/***************************************************************************/
/******************|          Support  Routines           |*****************/
/***************************************************************************/
system_cache_lookup(element_type, input, typlen, typbyval, typdelim, typelem, proc)
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

    if (!HeapTupleIsValid(typeTuple)) {
        elog(WARN, "array_out: Cache lookup failed for type %d\n",
             element_type);
        return NULL;
    }
    typeStruct = (TypeTupleForm) GETSTRUCT(typeTuple);
    *typlen    = typeStruct->typlen;
    *typbyval  = typeStruct->typbyval;
    *typdelim  = typeStruct->typdelim;
    *typelem   = typeStruct->typelem;
    if (input) {
        *proc = typeStruct->typinput;
    } else {
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

    if (typlen > 0) {
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
        } else {
            bcopy(src, dest, typlen);
        }
        inc = typlen;
    } else {
        bcopy(src, dest, * (int32 *) src);
        inc = (INTALIGN(* (int32 *) src));
    }
    return(inc);
} 

/***************************************************************************/
char * 
_AdvanceBy1word(str, word)
char *str, **word;
{
    char *retstr, *space;

    *word = NULL;
    if (str == NULL) return str;
    while (isspace(*str)) str++;
    *word = str;
    if ((space = (char *)index(str, ' ')) != (char *) NULL) {
        retstr = space + 1;
        *space = '\0';
    }
    else 
        retstr = NULL;
    return retstr;
}
    
/************************************************************************/
SanityCheckInput(ndim, n, dim, lb, indx)
int ndim, n, dim[], lb[], indx[];
{
    int i;
    /* Do Sanity check on input */
    if (n != ndim) return 0;
    for (i = 0; i < ndim; i++)
        if ((lb[i] > indx[i]) || (indx[i] >= (dim[i] + lb[i])))
            return 0;
    return 1;
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
    char *srcPtr, *array_seek();

    n = ARR_NDIM(array); dim = ARR_DIMS(array);
    lb = ARR_LBOUND(array); srcPtr = ARR_DATA_PTR(array);
    for (i = 0; i < n; st[i] -= lb[i], endp[i] -= lb[i], i++); 
    get_prod(n, dim, prod);
    st_pos = tuple2linear(n, st, prod);
    srcPtr = array_seek(srcPtr, bsize, st_pos);
    get_range(n, span, st, endp);
    get_offset_values(n, dist, prod, span);
    for (i=0; i < n; indx[i++]=0);
    i = j = n-1; inc = bsize;
    do {
        srcPtr = array_seek(srcPtr, bsize,  dist[j]); 
        if (from) 
            inc = array_read(destPtr, bsize, 1, srcPtr);
        else 
            inc = array_read(srcPtr, bsize, 1, destPtr);
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
    char *ptr, *array_seek();

    n = ARR_NDIM(array); dim = ARR_DIMS(array);
    lb = ARR_LBOUND(array); ptr = ARR_DATA_PTR(array);
    for (i = 0; i < n; st[i] = stI[i]-lb[i], endp[i]=endpI[i]-lb[i], i++);
    get_prod(n, dim, prod);
    st_pos = tuple2linear(n, st, prod);
    ptr = array_seek(ptr, -1, st_pos);
    get_range(n, span, st, endp);
    get_offset_values(n, dist, prod, span);
    for (i=0; i < n; indx[i++]=0);
    i = j = n-1;
    do {
        ptr = array_seek(ptr, -1,  dist[j]);
        inc =  INTALIGN(* (int32 *) ptr);
        ptr += inc; count += inc;
    } while ((j = next_tuple(i+1, indx, span)) != -1);
    return count;
}

/***********************************************************************/
char *
array_seek(ptr, eltsize, nitems)
int eltsize, nitems;
char *ptr;
{
    int i;

    if (eltsize > 0) 
        return(ptr + eltsize*nitems);
    for (i = 0; i < nitems; i++) 
          ptr += INTALIGN(* (int32 *) ptr);
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
      tmp = (INTALIGN(* (int32 *) srcptr));
      bcopy(srcptr, destptr, tmp);
      srcptr += tmp;
      destptr += tmp;
      inc += tmp;
    }
    return(inc);
}
/***********************************************************************/
_LOArrayRange(st, endp, bsize, srcfd, destfd, array, isSrcLO, isNull)
int st[], endp[], bsize, isSrcLO;
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
    st_pos = tuple2linear(n, st, prod);
    offset = st_pos*bsize;
    if (LOlseek(srcfd, offset, SEEK_SET) < 0) 
        return;
    get_range(n, span, st, endp);
    get_offset_values(n, dist, prod, span);
    for (i=0; i < n; indx[i++]=0);
    for (i = n-1, inc = bsize; i >= 0; inc *= span[i--])
        if (dist[i]) 
            break;
    j = n-1;
    do {
        offset += (dist[j]*bsize);
        if (LOlseek(srcfd,  offset, SEEK_SET) < 0) 
            return;
        tmp = _LOtransfer(&srcfd, inc, 1, &destfd, isSrcLO, 1);
        if ( tmp < inc )
             return;
        offset += inc;
    } while ((j = next_tuple(i+1, indx, span)) != -1);
}

/***********************************************************************/
_ReadArray (st, endp, bsize, srcfd, destfd, array, isDestLO, isNull)
int st[], endp[], bsize, isDestLO;
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
    st_pos = tuple2linear(n, st, prod);
    offset = st_pos*bsize;
    if (LOlseek(srcfd, offset, SEEK_SET) < 0)
        RETURN_NULL;
    get_range(n, span, st, endp);
    get_offset_values(n, dist, prod, span);
    for (i=0; i < n; indx[i++]=0);
    for (i = n-1, inc = bsize; i >= 0; inc *= span[i--])
        if (dist[i]) 
            break;
    j = n-1;
    do {
        offset += (dist[j]*bsize);
        if (LOlseek(srcfd,  offset, SEEK_SET) < 0) 
            RETURN_NULL;
        tmp = _LOtransfer(&destfd, inc, 1, &srcfd, 1, isDestLO);
        if ( tmp < inc ) 
            RETURN_NULL;
        offset += inc;
    } while ((j = next_tuple(i+1, indx, span)) != -1);
}

/***********************************************************************/
_LOtransfer(destfd, size, nitems, srcfd, isSrcLO, isDestLO)
char **destfd, **srcfd;
int size, nitems, isSrcLO, isDestLO;
{
#define MAX_READ (512 * 1024)
#define min(a, b) (a < b ? a : b)
    struct varlena *v;
    int tmp, inc, resid;

    inc = nitems*size; 
    if (isSrcLO && isDestLO && inc > 0)
	for (tmp = 0, resid = inc;
		 resid > 0 && (inc = min(resid, MAX_READ)) > 0; resid -= inc) { 
	    v = (struct varlena *) LOread((int) *srcfd, inc);
	    if (VARSIZE(v) - 4 < inc) 
		{pfree(v); return(-1);}
	    tmp += LOwrite((int) *destfd, v);    
	    pfree(v);
	
	} 
    else if (!isSrcLO && isDestLO) {
        tmp = LO_write(*srcfd, inc, (int) *destfd);
        *srcfd = *srcfd + tmp;
    } 
    else if (isSrcLO && !isDestLO) {
        tmp = LO_read(*destfd, inc, (int) *srcfd);
        *destfd = *destfd + tmp;
    } 
    else {
        bcopy(*srcfd, *destfd, inc);
        tmp = inc;
        *srcfd += inc;
        *destfd += inc;
    }
    return(tmp);
#undef MAX_READ
}

/***********************************************************************/
char *
_array_set(array, indx_str, dataPtr)
ArrayType *array;
struct varlena *indx_str, *dataPtr;
{
    int len, i;
    int indx[MAXDIM];
    char *retval, *s, *comma, *r;
    bool isNull;

    /* convert index string to index array */
    s = VARDATA(indx_str);
    for (i=0; *s != '\0'; i++) {
        while (isspace(*s))
            s++;
        r = s;
        if ((comma = (char *)index(s, ',')) != (char *) NULL) {
            *comma = '\0';
            s = comma + 1;
        } else 
            while (*s != '\0') 
                s++;
        if ((indx[i] = atoi(r)) < 0)
            elog(WARN, "array dimensions must be non-negative");
    }
    retval = array_set(array, i, indx, dataPtr, &isNull);
    return(retval);
}

/***************************************************************************/
char *
_array_newLO(fd, flag)
int *fd;
int flag;
{
    char *p;
    char saveName[NAME_LEN];

    p = (char *) palloc(NAME_LEN);
    sprintf(p, "/Arry.%d", newoid());
    strcpy (saveName, p);
    if ( (*fd = LOcreat (saveName, 0600, flag)) < 0)
       elog(WARN, "Large object create failed");
    return (p);
}
/**********************************************************************/
