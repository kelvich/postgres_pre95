typedef struct {
	int	size;				/* total array size  									*/
	int	ndim;				/* # of dimensions   									*/
	int flags;				/* bit mask of flags: 1st bit - large object flag       */
							/* 2nd bit - chunk flag, array is chunked if set        */
							/* 3rd bit - inversion flag - used only if bit 1 is set */
} ArrayType;


#define ARR_NDIM(a)    		 (a->ndim)
#define ARR_NDIM_PTR(a) 	 (&(a->ndim))
#define ARR_IS_LO(a)		 (a->flags & 0x1)
#define SET_LO_FLAG(f,a) 	 (a->flags |= ((f) ? 0x1 : 0x0))
#define ARR_IS_CHUNKED(a)	 (a->flags & 0x2)
#define SET_CHUNK_FLAG(f,a)	 (a->flags |= ((f) ? 0x2 : 0x0))
#define ARR_IS_INV(a)		 (a->flags & 0x4)
#define SET_INV_FLAG(f,a) 	 (a->flags |= ((f) ? 0x4 : 0x0))
#define ARR_DIMS(a)	         ((int *) (((char *)(a)) + sizeof(ArrayType)))
#define ARR_LBOUND(a)	     ((int *) (((char *)(a)) + sizeof(ArrayType) + \
                                       (sizeof(int) * (a)->ndim)))
#define ARR_DATA_PTR(a)	     (((char *) (a)) + sizeof(ArrayType) + 2 * \
								(sizeof(int) * (a)->ndim))
#define ARR_OVERHEAD(n)		 (sizeof(ArrayType) + 2*n*sizeof(int))
/*------------------------------------------------------------------------*/

#define RETURN_NULL {*isNull = true; return(NULL); }

/*-----------------------------------------------------------------------*/
#define NAME_LEN    30
#define MAX_BUFF_SIZE (1 << 18)

typedef struct {
	char  lo_name[NAME_LEN];
	int   C[MAXDIM];
} CHUNK_INFO;
