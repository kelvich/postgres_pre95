typedef struct {
	int	size;
	int	ndim;
	bool isLO;
} ArrayType;

#define ARR_NDIM(a)    		 (a->ndim)
#define ARR_NDIM_PTR(a) 	 (&(a->ndim))
#define ARR_IS_LO(a)		 (a->isLO)
#define ARR_IS_LO_PTR(a)	 (&(a->isLO))
#define ARR_DIMS(a)	         ((int *) (((char *)(a)) + sizeof(ArrayType)))
#define ARR_LBOUND(a)	     ((int *) (((char *)(a)) + sizeof(ArrayType) + \
                                       (sizeof(int) * (a)->ndim)))
#define ARR_DATA_PTR(a)	     (((char *) (a)) + sizeof(ArrayType) + 2 * \
								(sizeof(int) * (a)->ndim))
#define ARR_OVERHEAD(n)		 (sizeof(ArrayType) + 2*n*sizeof(int))
/*------------------------------------------------------------------------*/

#define GetOffset(offset, n, dim, lb, indx)   \
{                                               \
    int ii, scale;                                      \
    for (ii = n-1, scale = 1, offset = 0; ii >= 0; scale*=dim[ii--]) \
        offset += (indx[ii] - lb[ii])*scale; \
}
#define getNitems(ret, n, a) \
{   \
    int ii;     \
    for (ii = 0, ret = 1; ii < n; ret *= a[ii++]);  \
    if (n == 0) ret = 0;    \
}
#define get_offset_values(n, dist, PC, span) 						\
{																	\
	int def1_i, def1_j;												\
 	for (def1_j = n-2, dist[n-1]=0; def1_j  >= 0; def1_j--)			\
    for (def1_i = def1_j+1, dist[def1_j] = PC[def1_j]-1; def1_i < n;	\
        dist[def1_j] -= (span[def1_i] - 1)*PC[def1_i], def1_i++);	\
}																	
#define get_range(n, span, st, endp)								\
{																	\
	int def2_i;														\
	for (def2_i= 0; def2_i < n; def2_i++)							\
		span[def2_i] = endp[def2_i] - st[def2_i] + 1;				\
} 
#define get_prod(n, range, P)										\
{																	\
	int def3_i;														\
	for (def3_i= n-2, P[n-1] = 1; def3_i >= 0; def3_i--)			\
		P[def3_i] = P[def3_i+1] * range[def3_i + 1];				\
} 
#define tuple2linear(n, lin, tup, scale)							\
{																	\
	int def5_i;														\
	for (def5_i= lin = 0; def5_i < n; def5_i++)						\
		lin += tup[def5_i]*scale[def5_i];							\
} 
/*------------------------------------------------------------------------*/

#define isNumber(x)  ((x) <= '9' && (x) >= '0')
#define isSpace(x)   ((x) == ' ' || (x) == '\t')

/*------------------------------------------------------------------------*/

#define RETURN_NULL {*isNull = true; return(NULL); }
