/*--------------------------------------------------------------------------
 * array_utils.c :
 *     This file contains some support routines required for array functions
 *-------------------------------------------------------------------------
 */

int GetOffset(n, dim, lb, indx)  
int n, dim[], lb[], indx[];
{                                              
    int i, scale, offset;                                     
    for (i = n-1, scale = 1, offset = 0; i >= 0; scale*=dim[i--]) 
        offset += (indx[i] - lb[i])*scale;
    return offset ;
}

int getNitems(n, a)
int n, a[];
{    
    int i, ret;      
    for (i = 0, ret = 1; i < n; ret *= a[i++]);   
    if (n == 0) ret = 0;     
    return ret;
}

int compute_size(st, endp, n, base)
int st[], endp[], n, base;
{
    int i, ret;
    for (i = 0, ret = base; i < n; i++)
        ret *= (endp[i] - st[i] + 1);
    return ret;
}

void get_offset_values(n, dist, PC, span)
int n, dist[], PC[], span[];
{                                                                     
    int i, j;                                                         
     for (j = n-2, dist[n-1]=0; j  >= 0; j--)                         
    for (i = j+1, dist[j] = PC[j]-1; i < n;                             
        dist[j] -= (span[i] - 1)*PC[i], i++);                         
}                                                                 

void get_range(n, span, st, endp)
int n, span[], st[], endp[];
{                                                                     
    int i;                                                         
    for (i= 0; i < n; i++)                             
        span[i] = endp[i] - st[i] + 1;                 
} 

void get_prod(n, range, P)
int n, range[], P[];
{                                                                     
    int i;                                                         
    for (i= n-2, P[n-1] = 1; i >= 0; i--)             
        P[i] = P[i+1] * range[i + 1];                 
} 

int tuple2linear(n, tup, scale)
int n, tup[], scale[];
{
    int i, lin;
    for (i= lin = 0; i < n; i++)
        lin += tup[i]*scale[i];
    return lin;
} 

void array2chunk_coord(n, C, a_coord, c_coord)
int n, C[], a_coord[], c_coord[];
{
    int i;
    for (i= 0; i < n; i++)
         c_coord[i] = a_coord[i]/C[i];
}

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

    if (i) 
        return(i);
    if (curr[0]) 
        return(0);
    return(-1);
}
/**********************************************************************/
