/*
 * jbstruct.h : structs & defines for jukebox operations
 *
 * Copyright, supplied AS-IS, and all that.
 */
/* $Header$ */

#ifndef _TYPES_
#include <sys/types.h>
#endif

/*
 * jblib.a specific stuff
 */
/* JB_INIT() macro for clients */
#define JB_LIB_VERSION	"0.1"
#define JB_INIT() jb_init(JB_LIB_VERSION)

/* global variables */
extern int jb_server_port;		/* where to look for server */
extern char *jb_server_mach;		/* machine server runs on */

/*
 * more general stuff
 */
/* generic defines */
#define JB_MAX_SHELF		50	/* 50 shelves in jukebox */
#define JB_MAX_DRIVE		2	/* 2 worm drives */
#define JB_MAX_LOG_DRIVE	8	/* 8 logical drives */

/*
 * JBLABEL declaration
 *
 * This is storage for a label block.  "system" data occupies the first
 * 256 bytes, "user" data the rest (JB_USR_LABEL is the offset of the
 * first byte of the "user" field).
 *
 * Legal characters for label/format names are all ASCII chars except:
 *   0x00-0x1f 0x7f-0x9f 0xff '\0' ' ' '/' ':'
 *
 * There are two different kinds of label blocks, JB_LT_NORMAL, and
 * JB_LT_CONT.  NORMAL blocks have all fields as specified.  CONT blocks are
 * label continuation blocks; the "vl_start_block" field is then the
 * start block of a series of "vl_size" labels.  So if the first
 * JB_MAX_LABELS blocks are about to be used, then block JB_MAX_LABELS-2
 * becomes the continuation block, and another string of label blocks
 * is allocated on a different part of the platter.  "vl_kind" determines.
 *
 * Assumes alignment on (at most) 4-byte boundaries.
 * (some defs snarfed from "wp.h")
 */
#define JB_MAX_NAME	63	/* max length of a platter name */
#define JB_MAX_FORMAT	31	/* max length of format description */
#define JB_MAX_LABELS	100	/* max #of labels */
#define JB_USR_LABEL	256	/* byte offset of user area in label */
#define JB_BLOCK_SIZE	1024	/* size of a disk block on the platter */
#define JB_LT_NORMAL	0	/* normal label block */
#define JB_LT_CONT	1	/* continuation label block */
#define JB_MAGIC	0xd5690425

typedef union {
    u_char  vl_rawbytes[JB_BLOCK_SIZE];
    struct {
	u_long  vl_magic;		/* should always be JB_MAGIC */
	u_long  vl_unique;		/* unique based on Internet addr */
	u_long	vl_kind;		/* what kind of label block this is */
	time_t  vl_ctime;		/* creation time of this label */
	u_long	vl_side;		/* which side this is (0='A', 1='B') */
	u_long  vl_start_block;		/* first block; must >= JB_MAX_LABELS*/
	u_long  vl_size;		/* size of the disk, in blocks */
	char    vl_name[JB_MAX_NAME+1];	/* platter name */
	char    vl_format[JB_MAX_FORMAT+1];	/* format description */
    } vl_syslab;
} JBLABEL;


/*
 * JBPLATTER declaration (opaque to clients)
 *
 * Two of these per platter (one for each side).
 *
 * Returned by jb_open(), and used as an argument to most of the other
 * routines.
 */
typedef struct {
	int  shelf;			/* what shelf it's in */
	int  location;			/* (see below) */
	int  side;			/* which side this is 0=A, 1=B */
	int  access;			/* O_RDONLY or O_RDWR */
	int  locked;			/* locked in drive? */
	long offset;			/* virtual block 0 offset */
	int  ref_count;			/* #of people who have this open */
	JBLABEL label;			/* the whole 1K label */
} JBPLATTER;


/*
 * JBSTATUS declaration
 *
 * This is filled in by jb_istat() and jb_fstat().
 * Invalid fields will be null (strings) or JB_UNKNOWN (integer).
 * The only time all fields are guaranteed to be valid is when the
 * platter is open.
 *
 * Should be longword-aligned.
 */
typedef struct {
	int  location;
	int  access;
	int  locked;
	long offset;
	JBLABEL label;
} JBSTATUS;

/*
 * Constants
 */

/* value for unknown integers (note: MUST be zero) */
#define JB_UNKNOWN	0

/*
 * location (specific location info)
 *
 * 0-7 means that logical drive
 * JB_ONSHELF means it's sitting on the shelf specified by "shelf"
 * JB_OFFLINE means the platter is not in the jukebox
 */
#define JB_ONSHELF	0x8000	/* sitting on a shelf */
#define JB_OFFLINE	0x8001	/* not in the jukebox */

/* side */
#define JB_SIDE_A	0	/* this label is on side 'A' */
#define JB_SIDE_B	1	/* ditto for side 'B' */

/* locked */
#define JB_UNLOCKED	1	/* can be rescheduled freely */
#define JB_LOCKED	2	/* cannot be removed from drive */

/*
 * These constants are for the jb_open() call.  The value stored in the
 * "access" field of a JBPLATTER or JBSTATUS struct will be O_RDWR or
 * O_RDONLY, *not* JB_RDWR or JB_RDONLY.
 */
#define JB_RDONLY	0x0001	/* like O_RDONLY, but NOT equivalent */
#define JB_WRONLY	0x0002	/* ditto for O_WRONLY */
#define JB_RDWR		(JB_RDONLY | JB_WRONLY)


/*
 * error codes & detailed error info structs
 */

#define JB_ENOERR		0	/* no error - must be 0 */

/* problems with arguments */
#define JB_ENOSERVER		1	/* unable to connect to server */
#define JB_EUNKNOWN		2	/* platter name not recognized */
#define JB_EOFFLINE		3	/* platter is currently offline */
#define JB_EBADP		4	/* JBPLATTER pointer is bad */
#define JB_EBADFORMAT		5	/* mismatched format type */
#define JB_ERANGE		6	/* one of args was out of range */
#define JB_EISOPEN		7	/* platter (side) is already open */
#define JB_EACCES		8	/* JB_RDWR on write-protected platter*/

/* errors during operations */
#define JB_EIO			9	/* unspecified I/O error */
#define JB_ESERVER		10	/* communication failure with server */
#define JB_EBLANK		11	/* tried to read unwritten block */
#define JB_ENOTBLANK		12	/* tried to write written block */
#define JB_EINTERNAL		13	/* internal error; check jbserv.log */

/* problems with labels */
#define JB_ENOLABEL		14	/* tried to load unlabeled platter */
#define JB_ENAMECHANGE		15	/* tried to change name portion */
#define JB_ELABELED		16	/* can't RE-label from shelf 127 */
#define JB_EBADLABEL		17	/* some part of the label was bogus */
#define JB_ENOSPC		18	/* no room left for labels */

/* errors during debug-only operations */
#define JB_ENODEBUG		19	/* server isn't accepting debug reqs */
#define JB_EOFFSET		20	/* operation illegal b/c non-0 offset*/

/* total #of errors with text msgs (for jb_strerror); last error number +1 */
#define JB_NERR			21

