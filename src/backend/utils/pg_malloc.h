#include "pg_lisp.h"
#include "log.h"
#include "c.h"

#define MAPSIZE 72
#define FULL 0xff
#define BITS_IN_BYTE 8
#define TOTAL_NONNODE_PAGES 8
#define TRUE 1
#define FALSE 0
#define HEADER_TYPE 0xf0

/*
 * XXX Hack for palloc header info.
 */

#define PALLOC_HEADER_SIZE 16

/*
 * Each page is 4K in total (including header stuff) except for large pages.
 */

#define PAGE_SIZE 4*1024

/*
 * Allocate 128 pages initially for 1/2 meg of contiguous storage.
 */

#define PAGES_ALLOCATED 128

/*
 * Returns nonzero if the i'th bit of X is 1, zero otherwise.
 */

#define TEST_BIT(X, I)  ((X) & ((unsigned) 1 << (7 - (I))))

/*
 * Sets the i'th bit of X to 1 by or'ing it.
 */

#define SET_BIT(X, I)   ((X) | ((unsigned) 1 << (7 - (I)))) 

/*
 * Unsets the i'th bit of X by exclusive or'ing it.
 */

#define UNSET_BIT(X, I) ((X) ^ ((unsigned) 1 << (7 - (I))))

typedef struct
{
	long sanity_check;
	short debug_depth, size;
} NodeMemoryHeader;

/*
 * Memory page for NODES (and any objects that happen to be the same size
 * as nodes).  MAKE SURE THIS STRUCTURE IS ALWAYS A MULTIPLE OF FOUR IN SIZE!
 */

typedef struct node_mempage
{
	long sanity_check; /* Used as a sanity check - equal to &memory[1] */
	bool8 is_malloced, page_full;
	short ref_count, max_elements, element_size;
	long offset;
	NodeMemoryHeader *first_free_header;
	unsigned char *begin_addr, *end_addr;
	unsigned char page_map[MAPSIZE];
	struct node_mempage *next;
	unsigned char memory[1]; /* force contiguity */
}
NodeMemoryPage;


/* 
 * Memory page for non-nodes (meaning objects which are not the size of any
 * of the nodes in the node system).
 */

typedef struct 
{
	long sanity_check;
	short size;
	bool8 used;
	uint8 debug_depth;
}
OtherMemoryHeader;

/*
 * Memory page header for other (non-node) memory pages.
 *
 * When adding stuff to these, MAKE SURE THAT &memory[0] is
 * on a longword boundary!
 */

typedef struct other_mempage
{
	long sanity_check;
	bool8 is_malloced, page_full;
	short ref_count, total, biggest_free_object;
	unsigned char *begin_addr, *end_addr, *working_addr;
	OtherMemoryHeader *first_free_header, *last_header;
	struct other_mempage *next;
	unsigned char memory[1]; /* force contiguity */
}
OtherMemoryPage;

/*
 * For big pages (objects > about 3.9K).  For these, one object per page.
 */

typedef struct big_memory_page
{
	long sanity_check;
	struct big_memory_page *next;
	unsigned char *begin_addr, *end_addr;
	OtherMemoryHeader header;
	unsigned char memory[1];
}
BigMemoryPage;

/*
 * The node table.
 */

typedef struct
{
	char            **names;
	int             node_count;
	bool8  			used;
	Size            size;
	int             page_count;
	NodeMemoryPage *first_page;
}
NodeTable;

/*
 * The table of memory page data.  There will be three of these
 * when everything is complete: 
 * -- one for persistent objects
 * -- one for objects which disappear at end-transaction
 * -- one for portals
 */

typedef struct page_data
{
	unsigned char *my_space;
	long total_size, space_used;
	NodeTable *node_table;
	OtherMemoryPage *other_pages;
	BigMemoryPage *big_pages;
} MemoryPageData;

struct nodeinfo
{
    char    *ni_name;
	TypeId  ni_id;
	TypeId  ni_parent;
	Size    ni_size;
};

/* Function declarations */

/*
 * Allocates space.
 */

unsigned char *
pg_malloc ARGS((
Size size;
));

unsigned char *
GetNodeMemory ARGS((
Size size;
));

unsigned char *
GetOtherMemory ARGS((
Size size;
));

unsigned char *
GetBigMemory ARGS((
Size size;
));

NodeTable *
InitializeNodeTable ARGS((void));

NodeMemoryPage *
AllocateNodePage ARGS((
Size element_size;
));

OtherMemoryPage *
AllocateOtherPage ARGS((void));

BigMemoryPage *
AllocateBigPage ARGS((
Size size;
));

bool8
FreeNodeMemory ARGS((
unsigned char *addr;
));

bool8
FreeOtherMemory ARGS((
unsigned char *addr;
));

bool8
FreeBigMemory ARGS((
unsigned char *addr;
));

int
FindFreeElement ARGS((
unsigned char *page_map;
));

Size
LargestFreeObject ARGS((
OtherMemoryPage page;
));
