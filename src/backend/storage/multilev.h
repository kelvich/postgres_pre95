/*
 * multilev.h -- multi level lock table consts/defs
 *
 * for single.c and multi.c and their clients
 */

#define READ_LOCK  	2
#define WRITE_LOCK 	1

/* any time a small granularity READ/WRITE lock is set.  
 * Higher granularity READ_INTENT/WRITE_INTENT locks must
 * also be set.  A read intent lock is has value READ+INTENT.
 * in this implementation.
 */
#define NO_LOCK		0
#define INTENT		2
#define READ_INTENT	(READ_LOCK+INTENT)
#define WRITE_INTENT	(WRITE_LOCK+INTENT)

#define SHORT_TERM	1
#define LONG_TERM	2
#define UNLOCK		0

#define N_LEVELS 3
#define RELN_LEVEL 0
#define PAGE_LEVEL 1
#define TUPLE_LEVEL 2
typedef int LOCK_LEVEL;

