/*
 * large_object.h - file of info for Postgres large objects
 *
 * $Header$
 */

#include "tmp/c.h"

#define LARGE_OBJECT_BLOCK 1024

/*
 * This is the structure that goes in a tuple for large objects.
 */

typedef union
{
	char filename[1];
	char object_relname[1];
}
LargeObjectDataPtr;

/*
 * PURE_FILE indicates that a large object is strictly stored as a user
 * file and Postgres simply points to it.  This is good for large object
 * applications that use other files, but has no transaction semantics,
 * security, crash recover, etc.
 */

#define PURE_FILE	0 

/*
 * VERSIONED_FILE indicates that a large object is stored in the form of a
 * base object with a succeeding string of large object delta's, which are
 * new or replacement block numbers.
 */

#define VERSIONED_FILE	1 

/*
 * CLASS_FILE is a large object that is stored essentially as a Postgres
 * complex object.  Each block of the file is stored in a tuple.
 */

#define CLASS_FILE	2 

/*
 * Other large object schemes can be thought of - their numbering
 * should go here.
 *
 * !! VARIABLE LENGTH STRUCTURE !!
 */

typedef struct
{
/*
 * Length of this structure - this structure is treated as a Variable Length
 * Attribute by the Postgres access methods.
 */

	long lo_length;

/*
 * Large object storage type - described above.
 */
	long lo_storage_type;

/*
 * Ignored if the object is _not_ a versioned file
 */

	long lo_nblocks;
	short lo_lastoff, lo_version;

/*
 * Pointer to large object data - may be a filename or relation name
 */
	LargeObjectDataPtr lo_ptr;
}
LargeObject;

/*
 * This structure will eventually have lots more stuff associated with it.
 */

typedef struct
{
	int fd; /* Postgres internal file descriptor */
	LargeObject *object;
}
LargeObjectDesc;

/*
 * Function definitions...
 */

/*
 * external registered function interface 
 */

char *
lo_filein ARGS((
	char *filename
));


char *
lo_fileout ARGS((
    LargeObject *object
));

/* 
 * internal large object interface
 */

LargeObjectDesc *
LOCreate ARGS((
   int open_mode
));

LargeObjectDesc *
LOOpen ARGS((
    LargeObject *object,
    int open_mode
));

void
LOStat ARGS((
    LargeObjectDesc *obj_desc,
    unsigned int *nblocks, *byte_offset
));

int
LOBlockRead ARGS((
    LargeObjectDesc *obj_desc,
    char *buf,
    unsigned long nblocks
));

int
LOBlockWrite ARGS((
    LargeObjectDesc *obj_desc,
    char *buf,
    unsigned long n_whole_blocks, bytes_at_end
));

unsigned long
LOBlockSeek ARGS((
	LargeObjectDesc *obj_desc,
	unsigned long offset,
	int whence
));

void
LOClose ARGS((
    LargeObjectDesc *obj_desc
));

void
LODestroy ARGS((
    LargeObject *object
));

LargeObject *
LODescToObject ARGS((
    LargeObjectDesc *obj_desc
));
