/* --------------------------------------------------------------------------
 *   FILE
 *	array.h
 *
 *   DESCRIPTION
 *	Utilities for the new array code.
 *
 *   NOTES
 *	XXX the data array should be LONGALIGN'd -- notice that the array
 *	allocation code does not allocate the extra space required for this,
 *	even though the array-packing code does the LONGALIGNs.
 *
 *   IDENTIFICATION
 *	$Header$
 * --------------------------------------------------------------------------
 */

#ifndef ArrayHIncluded			/* include only once */
#define	ArrayHIncluded 1

typedef struct {
	int	size;	/* total array size (in bytes) */ 
	int	ndim;	/* # of dimensions */
	int	flags;	/* implementation flags */
} ArrayType;

/*
 * bitmask of ArrayType flags field:
 * 1st bit - large object flag
 * 2nd bit - chunk flag (array is chunked if set)
 * 3rd,4th,&5th bit - large object type (used only if bit 1 is set)
 */
#define	ARR_LOB_FLAG	(0x1)
#define	ARR_CHK_FLAG	(0x2)
#define	ARR_OBJ_MASK	(0x1c)

#define ARR_FLAGS(a)		((ArrayType *) a)->flags
#define	ARR_SIZE(a)		(((ArrayType *) a)->size)

#define ARR_NDIM(a)		(((ArrayType *) a)->ndim)
#define ARR_NDIM_PTR(a)		(&(((ArrayType *) a)->ndim))

#define ARR_IS_LO(a) \
	(((ArrayType *) a)->flags & ARR_LOB_FLAG)
#define SET_LO_FLAG(f,a) \
	(((ArrayType *) a)->flags |= ((f) ? ARR_LOB_FLAG : 0x0))

#define ARR_IS_CHUNKED(a) \
	(((ArrayType *) a)->flags & ARR_CHK_FLAG)
#define SET_CHUNK_FLAG(f,a) \
	(((ArrayType *) a)->flags |= ((f) ? ARR_CHK_FLAG : 0x0))

#define ARR_OBJ_TYPE(a) \
	((ARR_FLAGS(a) & ARR_OBJ_MASK) >> 2)
#define SET_OBJ_TYPE(f,a) \
	((ARR_FLAGS(a)&= ~ARR_OBJ_MASK), (ARR_FLAGS(a)|=((f<<2)&ARR_OBJ_MASK)))
#define ARR_IS_INV(a) (ARR_OBJ_TYPE(a) == Inversion)

/*
 * ARR_DIMS returns a pointer to an array of array dimensions (number of
 * elements along the various array axes).
 *
 * ARR_LBOUND returns a pointer to an array of array lower bounds.
 *
 * That is: if the third axis of an array has elements 5 through 10, then
 * ARR_DIMS(a)[2] == 6 and ARR_LBOUND[2] == 5.
 *
 * Unlike C, the default lower bound is 1.
 */
#define ARR_DIMS(a) \
	((int *) (((char *) a) + sizeof(ArrayType)))
#define ARR_LBOUND(a) \
	((int *) (((char *) a) + sizeof(ArrayType) + \
		  (sizeof(int) * (((ArrayType *) a)->ndim))))

/*
 * Returns a pointer to the actual array data.
 */
#define ARR_DATA_PTR(a) \
	(((char *) a) + sizeof(ArrayType) + 2 * (sizeof(int) * (a)->ndim))

/*
 * The total array header size for an array of dimension n (in bytes).
 */
#define ARR_OVERHEAD(n)	\
	(sizeof(ArrayType) + 2 * (n) * sizeof(int))

/*------------------------------------------------------------------------
 * Miscellaneous helper definitions and routines for arrayfuncs.c
 *------------------------------------------------------------------------
 */

#define RETURN_NULL {*isNull = true; return(NULL); }
#define NAME_LEN    30
#define MAX_BUFF_SIZE (1 << 18)

typedef struct {
	char  lo_name[NAME_LEN];
	int   C[MAXDIM];
} CHUNK_INFO;

#endif /* ArrayHIncluded */
