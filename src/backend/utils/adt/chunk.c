#include <ctype.h>
#include "tmp/postgres.h"
#include "tmp/align.h"
#include "tmp/libpq-fs.h"

#include "catalog/pg_type.h"
#include "catalog/pg_lobj.h"

#include "utils/palloc.h"
#include "fmgr.h"
#include "utils/log.h"
#include "array.h"
#include "planner/internal.h"

#define INFTY 500000000
#define MANY  10000
#define MAXPAT 20
#define quot_ceil(x,y)  (((x)+(y)-1)/(y))
#define min(x,y)        (((x) < (y))? (x) : (y))
#define max(x,y)        (((x) > (y))? (x) : (y))

CHUNK_INFO cInfo;

/*------------------------------------------------------------------------
 * _ChunkArray ---
 *     converts an input array to chunked format using the information 
 *     provided by the access pattern.
 * Results:
 *     creates a new file that stores the chunked array and returns 
 *     information about the chunked file
 *-----------------------------------------------------------------------
 */
char * _ChunkArray ( fd, afd, ndim, dim, baseSize, nbytes, fileFlag, chunkfile)
int fd, ndim, dim[], baseSize, *nbytes, fileFlag;
FILE *afd;
char *chunkfile;
{
    int cfd;
    int chunk[MAXDIM], csize;
    bool reorgFlag;
    char * _array_newLO();

    if (chunkfile == NULL) reorgFlag = true;
    else reorgFlag = false;

    if (reorgFlag) 
    /* create new LO for chunked file */
    chunkfile = _array_newLO( &cfd, fileFlag );
    else 
    cfd = LOopen(chunkfile, O_RDWR); 
    if (cfd < 0) elog(WARN, "Enable to open chunk file");
    strcpy (cInfo.lo_name, chunkfile);
    
    /* find chunk size */
    csize = GetChunkSize(afd, ndim, dim, baseSize, chunk);

    if (reorgFlag)
    /* copy data from input file to chunked file */
    _ConvertToChunkFile(ndim, baseSize, dim, chunk, fd, cfd);

    initialize_info(&cInfo, ndim, dim, chunk);
    *nbytes = sizeof(CHUNK_INFO);
    return (char *) &cInfo ;
}

/*--------------------------------------------------------------------------
 * Compute number of page fetches, given chunk dimension and access pattern 
 *   -- defined as a macro because it is called a zillion times by the 
 *      program that computes chunk size
 *------------------------------------------------------------------------
 */
#define compute_fetches(d, DIM, N, A, tc) \
{ \
    register int i,j, nc;\
    for (i = 0, tc = 0; i < N; i++){\
        for (j = 0, nc = 1; j < DIM; j++)\
            nc *= quot_ceil(A[i][j], d[j]); \
        nc *= A[i][DIM];\
        tc += nc;\
    }\
}

/*--------------------------------------------------------------------------
 * GetChunkSize --
 *        given an access pattern and array dimensionality etc, this program
 *      returns the dimensions of the chunk in "d"
 *-----------------------------------------------------------------------
 */

GetChunkSize( fd, ndim, dim, baseSize,  d)
int ndim, dim[MAXDIM], baseSize, d[MAXDIM];
FILE *fd;
{
    int N, i, j, numRead, csize;
    int A[MAXPAT][MAXDIM+1], dmax[MAXDIM];

    /* 
     * ----------- read input ------------ 
     */
    char * inpStr;

    fscanf(fd, "%d", &N);
    if ( N > MAXPAT )
        elog(WARN, "array_in: too many access pattern elements");
    for (i = 0; i < N; i++)
        for (j = 0; j < ndim+1; j++) 
            if (fscanf(fd, "%d ", &(A[i][j])) == EOF)
                elog (WARN, "array_in: bad access pattern input");
    
    /* 
     * estimate chunk size 
     */
    for (i = 0; i < ndim; i++)
        for (j = 0, dmax[i] = 1; j < N; j++)
        if (dmax[i] < A[j][i]) dmax[i] = A[j][i];
    csize = _PAGE_SIZE_/baseSize;
    
    _FindBestChunk (csize, dmax, d, ndim, A, N);

    return csize;    
}

/*-------------------------------------------------------------------------
 * _FindBestChunk --
 *        This routine does most of the number crunching to compute the 
 *        optimal chunk shape.
 * Called by GetChunkSize
 *------------------------------------------------------------------------
 */
_FindBestChunk (size, dmax, dbest, dim, A, N)
int size, dim, N, dbest[], dmax[], A[MAXPAT][MAXDIM+1];
{
    int d[MAXDIM];
    register int i,j;
    int tc, mintc = INFTY;

    d[0] = 0;
    mintc = INFTY;
    while (get_next(d,dim,size, dmax)) {
        compute_fetches(d, dim, N, A, tc);

        if (mintc >= tc) {
          mintc = tc;
          for (j = 0; j < dim; dbest[j] = d[j], j++);
        }
    }
    return(mintc);
}

/*----------------------------------------------------------------------
 * get_next --
 *   Called by _GetBestChunk to get the next tuple in the lexicographic order
 *---------------------------------------------------------------------
 */
get_next(d, k, C, dmax)
int d[], k, C, dmax[];
{
    register int i,j, temp;

    if (!d[0]) {
        temp = C;
        for (j = k-1; j >= 0; j--){
            d[j] = min(temp, dmax[j]);
            temp = max(1, temp/d[j]);
        }
        return(1);
    }

    for (j = 0, temp = 1; j < k; j++)
        temp *= d[j];

    for (i=k-1; i >= 0; i--){
        temp = temp/d[i];
        if (((temp*(d[i]+1)) < C) && (d[i]+1 <= dmax[i])) break;
    }
    if (i < 0) return(0);

    d[i]++;
    j = C/temp;
    d[i] = min(dmax[i], j/(j/d[i]));
    temp = temp*d[i];
    temp = C/temp;

   for (j = k-1; j > i; j--){
       d[j] = min(temp, dmax[j]);
        temp = max(1, temp/d[j]);
    }
    return(1);

}

char a_chunk[_PAGE_SIZE_ + 4];       /* 4 since a_chunk is in varlena format */

initialize_info(A, ndim, dim, chunk)
CHUNK_INFO *A;
int ndim, dim, chunk[];
{
    int i;

    for ( i = 0; i < ndim; i++)
        A->C[i] = chunk[i];
}

/*--------------------------------------------------------------------------
 * Procedure reorganize_data():
 *    This procedure reads the input multidimensional array that is organised
 *    in the order specified by array "X" and breaks it up into chunks of
 *    dimensions specified in "C". 
 *
 *    This is a very slow process, since reading and writing of LARGE files
 *    may be involved.
 *
 *-------------------------------------------------------------------------
 */

_ConvertToChunkFile(n, baseSize, dim, C, srcfd, destfd)
int n, baseSize, srcfd, destfd, dim[], C[];
{
    int max_chunks[MAXDIM], chunk_no[MAXDIM];
    int PX[MAXDIM], dist[MAXDIM];
    int csize = 1, i, temp;

    for (i = 0; i < n; chunk_no[i++] = 0) {
        max_chunks[i] = dim[i]/C[i];
        csize *= C[i];
    }
    csize *= baseSize;
    temp = csize + 4;
    bcopy(&temp, a_chunk, 4);

    get_prod(n, dim, PX);
    get_offset_values(n, dist, PX, C);
    for (i = 0; i < n; dist[i] *= baseSize, i++);

    do {
        read_chunk(chunk_no, C, &(a_chunk[4]), srcfd, n, baseSize, PX, dist); 
        write_chunk(a_chunk, destfd);
    }   while (next_tuple(n, chunk_no, max_chunks) != -1);
}

/*--------------------------------------------------------------------------
 * read_chunk
 *    reads a chunk from the input files into a_chunk, the position of the
 *    chunk is specified by chunk_no  
 *--------------------------------------------------------------------------
 */

read_chunk(chunk_no, C, a_chunk, srcfd, n, baseSize, PX, dist)
int chunk_no[], C[], srcfd, n, baseSize, PX[], dist[];
char a_chunk[];
{
    int i, j, cp, unit_transfer;
    int start_pos, pos[MAXDIM];
    int indx[MAXDIM];
    int fpOff;

    for ( i = start_pos = 0; i < n; i++) {
        pos[i] = chunk_no[i] * C[i];
        start_pos += pos[i]*PX[i];
    }
    start_pos *= baseSize;

    /* Read a block of dimesion C starting at co-ordinates pos */
    unit_transfer = C[n-1] * baseSize;

    for (i = 0; i < n; indx[i++] = 0);
    fpOff = start_pos;
    seek_and_read(fpOff, unit_transfer, a_chunk, srcfd, L_SET);
    fpOff += unit_transfer;
    cp = unit_transfer;

    while ((j = next_tuple(n-1, indx, C)) != -1) {
        fpOff += dist[j];
        seek_and_read(fpOff, unit_transfer, &(a_chunk[cp]), srcfd, L_SET);
        cp += unit_transfer;
        fpOff += unit_transfer;
    }
}

/*--------------------------------------------------------------------------
 * write_chunk()
 *    writes a chunk of size csize into the output file
 *--------------------------------------------------------------------------
 */
write_chunk(a_chunk, ofile)
struct varlena * a_chunk;
int ofile;
{
    int     got_n;
    got_n = LOwrite (ofile, a_chunk);
    return(got_n);
}

/*--------------------------------------------------------------------------
 * seek_and_read()
 *    seeks to the asked location in the input file and reads the 
 *    appropriate number of blocks
 *   Called By: read_chunk()
 *--------------------------------------------------------------------------
 */
seek_and_read(pos, size, buff, fp, from )
char buff[];
int pos, size;
int from;
int fp;
{
    int fno;
    struct varlena *v;

    /* Assuming only one file */
    if ( LOlseek(fp, pos, from ) < 0) elog(WARN, "File seek error");
    v = (struct varlena *) LOread(fp, size);
    if (VARSIZE(v) - 4 < size) elog(WARN, "File read error");
    bcopy(VARDATA(v), buff, size);
    pfree(v);
    return(1);

}
/*----------------------------------------------------------------------------*/
#define NEXT_TUPLE(n, curr,  span, j)                               \
{                                                                   \
    int i;                                                          \
    if (!(n)) j = -1;                                               \
    else {                                                          \
        curr[n-1] = (curr[n-1]+1)%span[n-1];                        \
        for (i = n-1; i*(!curr[i]); i--)                            \
            curr[i-1] = (curr[i-1]+1)%span[i-1];                    \
        if (i) j = i; else if (curr[0]) j = 0; else j = -1;         \
    }                                                               \
}

/*----------------------------------------------------------------------------
 * _ReadChunkArray --
 *        returns the subarray specified bu the range indices "st" and "endp"
 *        from the chunked array stored in file "fp"
 *---------------------------------------------------------------------------
 */
_ReadChunkArray ( st, endp, bsize, fp, destfp, array, isDestLO, isNull )
int st[], endp[], isDestLO, bsize;
int fp;
char * destfp;
ArrayType *array;
bool *isNull;
{
    int i,j,jj;
    int n, got_n, temp, words_read;
    int chunk_span[MAXDIM], chunk_off[MAXDIM]; 
    int chunk_st[MAXDIM], chunk_end[MAXDIM];
    int block_seek;

    int  bptr, cptr, *C, csize, *dim, *lb;
    int range_st[MAXDIM], range_end[MAXDIM], 
        range[MAXDIM], array_span[MAXDIM];
    int PA[MAXDIM], PCHUNK[MAXDIM], PC[MAXDIM];
    int  to_read;
    int cdist[MAXDIM], adist[MAXDIM]; 
    int dist[MAXDIM], temp_seek;

    int destOff, srcOff;        /* Needed since LO don't understand SEEK_CUR*/
    char *baseDestFp = (char *)destfp;
    
    CHUNK_INFO *A = (CHUNK_INFO *) ARR_DATA_PTR(array);
    n = ARR_NDIM(array); 
    dim = ARR_DIMS(array);
    lb = ARR_LBOUND(array); 
    C = A->C;

    csize = C[n-1];
    PC[n-1] = 1;
    temp = dim[n - 1]/C[n-1];
    for (i = n-2; i >= 0; i--){
        PC[i] = PC[i+1] * temp;
        temp = dim[i] / C[i];
        csize *= C[i];
    }

    for (i = 0; i < n; st[i] -= lb[i], endp[i] -= lb[i], i++);
    get_prod(n, C, PCHUNK);
    get_range(n, array_span, st, endp);
    get_prod(n, array_span, PA);

    array2chunk_coord(n, C, st, chunk_st);
    array2chunk_coord(n, C, endp, chunk_end);
    get_range(n, chunk_span, chunk_st, chunk_end);
    get_offset_values(n, dist, PC, chunk_span);

    for (i = 0; i < n; i++) {
        range_st[i] = st[i];
        range_end[i] = min(chunk_st[i]*C[i]+C[i]-1, endp[i]);
    }

    for (i = j = 0; i < n; i++)
        j+= chunk_st[i]*PC[i];
    temp_seek = srcOff = j * csize * bsize;
    if (LOlseek(fp, srcOff, L_SET) < 0) RETURN_NULL;
    
    jj = n-1;
    for (i = 0; i < n; chunk_off[i++] = 0);
    words_read = 0; temp_seek = 0;
    do {
        /* Write chunk (chunk_st) to output buffer */
        get_range(n, array_span,  range_st, range_end);
        get_offset_values(n, adist, PA, array_span);
        get_offset_values(n, cdist, PCHUNK, array_span);
        for (i=0; i < n; range[i] = range_st[i]-st[i], i++);
        bptr = tuple2linear(n, range, PA);
        for (i = 0; i < n; range[i++] = 0);
        j = n-1; bptr *= bsize;
        if (isDestLO) { 
            if (LOlseek(destfp, bptr, L_SET) < 0) RETURN_NULL;
        }
        else 
            destfp = baseDestFp + bptr; 
        for(i = 0, block_seek = 0; i < n; i++)
            block_seek += (range_st[i]-(chunk_st[i] + chunk_off[i])
                                                *C[i])*PCHUNK[i];
        if (dist[jj] + block_seek + temp_seek) {
            temp = (dist[jj]*csize+block_seek+temp_seek)*bsize;
            srcOff += temp;
            if (LOlseek(fp, srcOff, L_SET) < 0) RETURN_NULL;
        }
        for (i = n-1, to_read = bsize; i >= 0; 
                to_read *= min(C[i], array_span[i]), i--)
            if (cdist[i] || adist[i]) break;
        do {
            if (cdist[j]) {
                srcOff += (cdist[j]*bsize);
                if (LOlseek(fp, srcOff, L_SET) < 0) RETURN_NULL;
            }
            block_seek += cdist[j];
            bptr += adist[j]*bsize;
            if (isDestLO) { 
                if (LOlseek(destfp, bptr, L_SET) < 0) RETURN_NULL;
            }
            else 
                destfp = baseDestFp + bptr;
            temp = _LOtransfer (&destfp, to_read, 1, &fp, 1, isDestLO);
            if (temp < to_read) RETURN_NULL;
            srcOff += to_read;
            words_read+=to_read;
            bptr += to_read;
            block_seek += (to_read/bsize);
            NEXT_TUPLE(i+1, range, array_span, j);
        } while (j != -1);    

         block_seek = csize - block_seek;    
        temp_seek = block_seek;
        jj = next_tuple(n, chunk_off, chunk_span);
        if (jj == -1) break;
        range_st[jj] = (chunk_st[jj]+chunk_off[jj])*C[jj];
        range_end[jj] = min(range_st[jj] + C[jj]-1, endp[jj]);
        
        for (i = jj+1; i < n; i++) {
        range_st[i] = st[i];
        range_end[i] = min((chunk_st[i]+chunk_off[i])*C[i]+C[i]-1, endp[i]);
        }
    } while (jj != -1);
    return(words_read);
}

/*------------------------------------------------------------------------
 * _ReadChunkArray1El --
 *       returns one element of the chunked array as specified by the index "st"
 *       the chunked file descriptor is "fp"
 *-------------------------------------------------------------------------
 */
struct varlena *
_ReadChunkArray1El ( st, bsize, fp, array, isNull )
int st[], bsize;
int fp;
ArrayType *array;
bool *isNull;
{
    int i, j, n, temp, srcOff;
    int chunk_st[MAXDIM];

    int  *C, csize, *dim, *lb;
    int PCHUNK[MAXDIM], PC[MAXDIM];
    struct varlena *v;

    CHUNK_INFO *A = (CHUNK_INFO *) ARR_DATA_PTR(array);

    n = ARR_NDIM(array); 
    lb = ARR_LBOUND(array); 
    C = A->C;
    dim = ARR_DIMS(array);

    csize = C[n-1];
    PC[n-1] = 1;
    temp = dim[n - 1]/C[n-1];
    for (i = n-2; i >= 0; i--){
        PC[i] = PC[i+1] * temp;
        temp = dim[i] / C[i];
        csize *= C[i];
    }

    for (i = 0; i < n; st[i] -= lb[i], i++);
    get_prod(n, C, PCHUNK);

    array2chunk_coord(n, C, st, chunk_st);

    for (i = j = 0; i < n; i++)
        j+= chunk_st[i]*PC[i];
    srcOff = j * csize;
    
    for(i = 0; i < n; i++)
        srcOff += (st[i]-chunk_st[i]*C[i])*PCHUNK[i];

    srcOff *= bsize;
    if (LOlseek(fp, srcOff, L_SET) < 0) RETURN_NULL;

    return (struct varlena *) LOread(fp, bsize);
}
