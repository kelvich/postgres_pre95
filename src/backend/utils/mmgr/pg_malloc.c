#include "tmp/c.h"

RcsId("$Header$");

#include "access/htup.h"
#include "tmp/align.h"
#include "utils/log.h"
#include "utils/pg_malloc.h"

#include "nodes/pg_lisp.h"

static bool8 memory_is_initialized = FALSE;
static int number_of_nodes = 0;
extern struct nodeinfo _NodeInfo[]; 

static MemoryPageData xact_page_data = {NULL, 0, NULL, NULL, NULL};
static MemoryPageData global_page_data = {NULL, 0, NULL, NULL, NULL};

static MemoryPageData *page_data = &xact_page_data;

static bool8 memory_tracing_mode = FALSE;
static uint8 trace_depth = 0;

static NodeTable *node_table = NULL;

#define HeaderIsValid(header) \
    ((header) != NULL \
     && (header)->sanity_check == (long) (header) + sizeof(*(header)))

#define PageIsValid(page) \
    (((page) != NULL) && ((page)->sanity_check == (long) (page)->begin_addr))

#define NextOtherHeader(header) \
    ((OtherMemoryHeader *) (((long) (header)) \
     + sizeof(OtherMemoryHeader) + (header)->size))

#define NextNodeHeader(header, size) \
    ((NodeMemoryHeader *) (((long) (header)) \
     + sizeof(NodeMemoryHeader) + size))

#define NodeTableHash(size) \
	((LONGALIGN(size) - PALLOC_HEADER_SIZE) / 4 - 1)

#define SizeIsNodeSize(size) \
	(NodeTableHash(size) > -1 && NodeTableHash(size) < number_of_nodes)
/*
 *******************************
 * EXTERNAL INTERFACE ROUTINES *
 *******************************
 */

unsigned char *
pg_malloc(size)

Size size;

{
    unsigned char *retval = NULL;

    if (!memory_is_initialized) {
        InitMemoryPages();
        memory_is_initialized = TRUE;
    }

/*
 * Force all returned addresses to be on long boundaries.
 */
 
    size = LONGALIGN(size);

/*
 * GetNodeMemory returns NULL if size is not really a node size
 */

    if (SizeIsNodeSize(size)) retval = GetNodeMemory(size);

/*
 * GetOtherMemory returns NULL if size is too big to fit into a page
 */

    if (retval == NULL) retval = GetOtherMemory(size);

    if (retval == NULL) retval = GetBigMemory(size);

    return(retval);
}

pg_free(addr)

unsigned char *addr;

{
    if (FreeNodeMemory(addr))
        return;
    else if (FreeOtherMemory(addr))
        return;
    else if (FreeBigMemory(addr))
        return;
    else
    {
        elog(NOTICE, "Address %x was not allocated with pg_malloc", addr);
        elog(NOTICE, "Address will be freed using free()");
        free(addr);
        return;
    }
}

/*
 * Zeros out all the pages.  It frees all the pages which were allocated with
 * malloc and initializes all others.
 */

pg_zero_memory()

{
    NodeMemoryPage *npage, *ntrailer;
    OtherMemoryPage *opage, *otrailer;
    BigMemoryPage *bpage, *btrailer;

	int i;

	for (i = 0; i < number_of_nodes; i++)
	{
		if (xact_page_data.node_table[i].used)
		{
			for (npage = npage = xact_page_data.node_table[i].first_page;
				 npage != NULL;
				 npage = npage->next)
			{
        		if (npage->is_malloced)
        		{
            		ntrailer->next = npage->next;
            		free(npage);
            		npage = ntrailer;
        		}
        		else
        		{
            		npage->ref_count = 0;
            		npage->page_full = FALSE;
            		bzero(npage->page_map, MAPSIZE);
        		}
        		ntrailer = npage;
			}
		}
    }

    for (opage = xact_page_data.other_pages;
         opage != NULL;
         opage = opage->next)
    {
        if (opage->is_malloced)
        {
            otrailer->next = opage->next;
            free(opage);
            opage = otrailer;
        }
        else
        {
            opage->ref_count = 0;
            opage->total = 0;
            opage->working_addr = opage->begin_addr;
            opage->page_full = FALSE;
            opage->biggest_free_object = 0;
        }
    }

    if (xact_page_data.big_pages != NULL)
    {
        for (bpage = xact_page_data.big_pages;
             bpage->next != NULL; /* do nothing */ )
        {
            btrailer = bpage->next;
            free(bpage);
            bpage=btrailer;
        }
        free(bpage);

        xact_page_data.big_pages = NULL;
    }
}

/*
 ********************
 * PRIVATE ROUTINES *
 ********************
 */

/*
 ****************
 * INITIALIZERS *
 ****************
 */

/*
 * This routine initializes the predefined memory pages.
 */

InitMemoryPages()

{
    NodeMemoryPage *scanner;
    OtherMemoryPage *other_scanner;
	NodeTable *node_table;
    long i, j;
    bool8 entering = TRUE;
    Size size;

    xact_page_data.total_size = PAGE_SIZE * PAGES_ALLOCATED;
    xact_page_data.space_used = 0;
    xact_page_data.my_space = (unsigned char *)
                              malloc(xact_page_data.total_size);
    if (xact_page_data.my_space == NULL) {
        perror("malloc");
        elog(WARN, "Memory manager (InitMemoryPages) - MALLOC FAILED");
    }

/*
 * Initialize the first table of memories
 */

    node_table = InitializeNodeTable();

    for (i = 0; i < number_of_nodes; i++)
    {
		if (node_table[i].used)
		{
			node_table[i].first_page = AllocateNodePage(node_table[i].size);
			scanner = node_table[i].first_page;
			for (j = 1; j < node_table[i].page_count; j++)
			{
				scanner->next = AllocateNodePage(node_table[i].size);
				scanner = scanner->next;
			}
		}
	}

	xact_page_data.node_table = node_table;

    entering = TRUE;
    for (i = 0; i < TOTAL_NONNODE_PAGES; i++)
    {
        if (entering)
        {
            xact_page_data.other_pages = AllocateOtherPage();
            other_scanner = xact_page_data.other_pages;
            entering = FALSE;
        }
        else
        {
            other_scanner->next = AllocateOtherPage();
            other_scanner = other_scanner->next;
        }
    }
    xact_page_data.big_pages = NULL;
}

/*
 * This routine initializes the node page information - it cannot use
 * elog because it is called too early and elog aborts.
 *
 * Eventually, the heuristics for memory usage will be passed as an
 * argument.
 */

NodeTable *
InitializeNodeTable()

{
    int i, j, position, size;
	NodeTable *node_table;

    for (i = 0; _NodeInfo[i].ni_size != 0; i++);
    number_of_nodes = i;

	node_table = (NodeTable *) malloc(number_of_nodes * sizeof(NodeTable));

	for (i = 0; i < number_of_nodes; i++)
	{
		node_table[i].used = FALSE;
		node_table[i].node_count = 0;
		node_table[i].names = NULL;
		node_table[i].size = 0;
		node_table[i].page_count = 0;
		node_table[i].first_page = NULL;
	}

	for (i = 0; i < number_of_nodes; i++)
	{
		size = _NodeInfo[i].ni_size + PALLOC_HEADER_SIZE;
		position = NodeTableHash(size);
		if (position >= number_of_nodes)
		{
			sprintf(stderr, "InitializeNodeTable - hash failed for %s",
					_NodeInfo[i].ni_name);
			sprintf(stderr, "Non-node memory will be used.");
		}
		else
		{
			if (!node_table[position].used) node_table[position].used = TRUE;
			node_table[position].node_count += 1;
			node_table[position].size = LONGALIGN(size);
/*
 * This should be assigned to the right value once we start doing memory
 * heuristics.  For now, we will just count the number of nodes of the
 * same size, and allocate that many pages per node.
 */

			node_table[position].page_count += 1;
		}
	}

/* 
 * Now we set up the name table information for use by the trace functions.
 */

	for (i = 0; i < number_of_nodes; i++)
	{
		size = _NodeInfo[i].ni_size + PALLOC_HEADER_SIZE;
		position = NodeTableHash(size);
		if (node_table[position].names == NULL)
		{
			size = node_table[position].node_count * sizeof(char *);
			node_table[position].names = (char **) malloc(size);
			for (j = 0; j < node_table[position].node_count; j++)
				node_table[position].names[j] = NULL;
		}
		for (j = 0; node_table[position].names[j] != NULL; j++);

		size = strlen(_NodeInfo[i].ni_name) + 1;
		node_table[position].names[j] = (char *) malloc(size);
		strcpy(node_table[position].names[j], _NodeInfo[i].ni_name);
	}
	return(node_table);
}

unsigned char *
GetNodeMemory(size)

Size size;

{
    NodeMemoryPage *scanner, *last_page, *this_page;
    NodeMemoryHeader *header;
    unsigned char *addr = NULL;
    bool8 size_is_node_size = FALSE;
    int offset, i, j, hash_position;

/*
 * See if the hash table entry for this size is really a node.  If it is,
 * find the first page with free elements in it.  Then compute the address.
 */

	hash_position = NodeTableHash(size);

	if (!page_data->node_table[hash_position].used) return(NULL);

    for (scanner = page_data->node_table[hash_position].first_page;
         scanner != NULL && addr == NULL;
         scanner = scanner->next)
    {
        if (!PageIsValid(scanner)) {
            elog(WARN, "GetNodeMemory - page at %x corrupted!", scanner);
        }

        if (scanner->element_size == size && !scanner->page_full)
        {
            header = scanner->first_free_header;
            addr = (unsigned char *)
                   ((long) header + sizeof(NodeMemoryHeader));
            header->debug_depth = trace_depth;
            SetElement(scanner->page_map, scanner->offset);
            scanner->ref_count += 1;
            SetNextFreeNode(scanner, scanner->offset);
            this_page = scanner;
        }
/*
 * In case we have to allocate a new page, set last_page to scanner.
 */
        if (scanner->next == NULL) last_page = scanner;
    }

    if (addr != NULL)
    {
        if (this_page->ref_count == this_page->max_elements)
        {
            this_page->page_full = TRUE;
            this_page->first_free_header = NULL;
        }
    }

/*
 * Here we have to allocate a new page and we set this object to the first
 * element of the new page.
 */

    if (addr == NULL)
    {
        last_page->next = AllocateNodePage(size);
        this_page = last_page->next;
        this_page->page_map[0] = SET_BIT(this_page->page_map[0], 0);
        header = (NodeMemoryHeader *) this_page->begin_addr;
        addr = this_page->begin_addr + sizeof(NodeMemoryHeader);
        header->debug_depth = trace_depth;
        this_page->ref_count = 1;
        this_page->first_free_header = (NodeMemoryHeader *)
                                       ((long) header
                                        + sizeof(NodeMemoryHeader)
                                        + this_page->element_size);
		this_page->offset = 1;
    }
    return(addr);
}

unsigned char *
GetOtherMemory(size)

Size size;

{
    OtherMemoryPage *page, *last_page, *this_page;
    OtherMemoryHeader *header, *scanhdr;
    unsigned char *addr = NULL, scan_addr;
    int offset, i;

    if (size > PAGE_SIZE - sizeof(OtherMemoryPage) - sizeof(char)) return(NULL);

    for (page = page_data->other_pages;
         page != NULL && addr == NULL;
         page = page->next)
    {

        if (!page->page_full)
        {
            if (page->biggest_free_object >= size)
            {
                if (page->first_free_header->size >= size)
                {
                    header = page->first_free_header;
                    if (page->total - page->ref_count > 1) /* at least one */
                    {
                        scanhdr = NextOtherHeader(header);
                        while (scanhdr->used)
                        {
                            if (!HeaderIsValid(scanhdr))
                            {
                                other_memory_page_info(page);
                                elog(WARN, "GetOtherMemory: page corrupted!");
                            }
                            scanhdr = NextOtherHeader(scanhdr);
                        }
                        page->first_free_header = scanhdr;
                    }
                }
                else
                {
                    scanhdr = page->first_free_header;
                    while (scanhdr->used || scanhdr->size < size)
                    {
                        scanhdr = NextOtherHeader(scanhdr);
                        if (!HeaderIsValid(scanhdr))
                        {
                            other_memory_page_info(page);
                            elog(WARN, "GetOtherMemory: page corrupted!");
                        }
                    }
                    header = scanhdr;
                }
                if (!HeaderIsValid(header))
                    elog(WARN, "GetOtherMemory: page corrupted!");
                page->ref_count += 1;
                header->used = TRUE;
                addr = (unsigned char *) header + sizeof(OtherMemoryHeader);

                if (page->ref_count == page->total)
                {
                    page->first_free_header = NULL;
                    page->biggest_free_object = 0;
                }
                else if (header->size == page->biggest_free_object)
                {
                    page->biggest_free_object = LargestFreeObject(page);
                    if (page->biggest_free_object == 0)
                    {
                        other_memory_page_info(page);
                        elog (WARN, "GetOtherMemory: free object scan failed");
                    }
                }
                this_page = page;
            }
            else if (page->working_addr + size + sizeof(OtherMemoryHeader)
              < page->end_addr)
            {
                header = (OtherMemoryHeader *) page->working_addr;
                header->sanity_check = (long) header
                                     + sizeof(OtherMemoryHeader);
                header->size = size;
                header->used = TRUE;
                header->debug_depth = trace_depth;
                addr = page->working_addr + sizeof(OtherMemoryHeader);
                page->ref_count += 1;
                page->total += 1;
                page->working_addr += size + sizeof(OtherMemoryHeader);
                page->last_header = header;
                this_page = page;
            }
        }
        if (page->next == NULL) last_page = page;
    }

/*
 * Here we need to allocate a new page
 */

    if (addr == NULL)
    {
        last_page->next = AllocateOtherPage();
        this_page = last_page->next;
        header = (OtherMemoryHeader *) this_page->begin_addr;
        header->sanity_check = (long) header + sizeof(OtherMemoryHeader);
        header->size = size;
        header->used = TRUE;
        header->debug_depth = trace_depth;
        this_page->last_header = header;
        this_page->total = this_page->ref_count = 1;
        this_page->working_addr = this_page->begin_addr
                           + size + sizeof(OtherMemoryHeader);
        addr = (unsigned char *) header + sizeof(OtherMemoryHeader);
    }

/*
 * Here we figure out if the page is full.
 */
    if ((this_page->end_addr - this_page->working_addr <
        sizeof(OtherMemoryHeader) + 1)
     && (this_page->total == this_page->ref_count))
    {
        this_page->page_full = TRUE;
    }
    return(addr);
}

unsigned char *
GetBigMemory(size)

Size size;

{
    BigMemoryPage *page;

    if (page_data->big_pages == NULL)
    {
        page = page_data->big_pages = AllocateBigPage(size);
    }
    else
    {
        for (page = page_data->big_pages;
             page->next != NULL;
             page = page->next);
        page->next = AllocateBigPage(size);
        page = page->next;
    }
    return((unsigned char *) &(page->memory[0]));
}

/*
 ***********************
 * ALLOCATION ROUTINES *
 ***********************
 */

/*
 * This could handle end of page overruns in a smarter way, but we'll get
 * this to work first.
 */

NodeMemoryPage *
AllocateNodePage(element_size)

Size element_size;

{
    NodeMemoryPage *page;
    int i, total_element_size;
    NodeMemoryHeader *header;

    if (page_data->space_used + PAGE_SIZE > page_data->total_size)
    {
        page = (NodeMemoryPage *) malloc(PAGE_SIZE);
        if (page == NULL)
        {
            perror("malloc");
            elog(WARN, "Memory manager (AllocateNodePage) - MALLOC FAILED");
        }
        page->is_malloced = TRUE;
    }
    else
    {
        page = (NodeMemoryPage *) (page_data->my_space + page_data->space_used);
        page_data->space_used += PAGE_SIZE;
        page->is_malloced = FALSE;
    }
    page->ref_count = 0;
    page->begin_addr = &(page->memory[0]);
    page->end_addr = (unsigned char *) page + PAGE_SIZE - 1;
    page->element_size = element_size;
    total_element_size = element_size + sizeof(NodeMemoryHeader);
    page->max_elements = (page->end_addr - page->begin_addr)
                       / total_element_size;

    for (i = 0, header = (NodeMemoryHeader *) page->begin_addr;
         i < page->max_elements;
         i++, header = NextNodeHeader(header, element_size))
    {
        header->sanity_check = (long) header + sizeof(NodeMemoryHeader);
        header->debug_depth = 0;
		header->size = element_size;
    }

    if (page->max_elements > MAPSIZE * BITS_IN_BYTE)
        page->max_elements = MAPSIZE * BITS_IN_BYTE;

    page->page_full = FALSE;
    bzero(page->page_map, MAPSIZE);
    page->next = NULL;
    page->sanity_check = (long) (page->begin_addr);
    page->first_free_header = (NodeMemoryHeader *) page->begin_addr;
    return(page);
}

OtherMemoryPage *
AllocateOtherPage()

{
    OtherMemoryPage *page;
    OtherMemoryHeader *header;

    if (page_data->space_used + PAGE_SIZE > page_data->total_size)
    {
        page = (OtherMemoryPage *) malloc(PAGE_SIZE);
        if (page == NULL)
        {
            perror("malloc");
            elog(WARN, "Memory manager (AllocateOtherPage) - MALLOC FAILED");
        }
        page->is_malloced = TRUE;
    }
    else
    {
        page = (OtherMemoryPage *)
               (page_data->my_space + page_data->space_used);

        page_data->space_used += PAGE_SIZE;
        page->is_malloced = FALSE;
    }
    page->page_full = FALSE;

    page->ref_count = page->total = 0;
    page->biggest_free_object = 0;
    page->first_free_header = page->last_header = NULL;

    page->begin_addr = page->working_addr = &(page->memory[0]);
    page->end_addr = (unsigned char *) page + PAGE_SIZE - 1;
    page->next = NULL;
    page->sanity_check = (long) (page->begin_addr);
    return(page);
}

BigMemoryPage *
AllocateBigPage(size)

Size size;

{
    BigMemoryPage *page;
    Size page_size;

    page_size = sizeof(BigMemoryPage) + size - sizeof(char *);

    page = (BigMemoryPage *) malloc(page_size);
    if (page == NULL)
    {
        perror("malloc");
        elog(WARN, "Memory manager (AllocateBigPage) - MALLOC FAILED");
    }
    page->begin_addr = &(page->memory[0]);
    page->end_addr = (unsigned char *) page->begin_addr + size - 1;
    page->next = NULL;
    page->sanity_check = (long) (page->begin_addr);
	page->header.sanity_check = (long) page->begin_addr;
	page->header.size = 32767;
	page->header.used = TRUE;
	page->header.debug_depth = trace_depth;
    return(page);
}

/*
 ********************
 * FREEING ROUTINES *
 ********************
 */

bool8
FreeNodeMemory(addr)

unsigned char *addr;

{
    NodeMemoryPage *page, *prev = NULL;
    NodeMemoryHeader *header;
    int offset, index, bit, hash_position;
    bool8 freed = FALSE;

    header = (NodeMemoryHeader *) (addr - sizeof(NodeMemoryHeader));

/*
 * Here, the header may well not be valid.  It should not be for non-node
 * objects.
 */

	if (HeaderIsValid(header))
	{
		hash_position = NodeTableHash(header->size);
		if (page_data->node_table[hash_position].used)
		{
    		for (page = page_data->node_table[hash_position].first_page;
         		 page != NULL && !freed;
         		 page = page->next)
    		{
        		if (page->begin_addr <= addr && page->end_addr > addr)
        		{
            		offset = (addr - page->begin_addr)
                   		   / (page->element_size + sizeof(NodeMemoryHeader));

            		index = offset / BITS_IN_BYTE;
            		bit = offset % BITS_IN_BYTE;

            		if (TEST_BIT(page->page_map[index], bit))
            		{
                		page->page_map[index] = UNSET_BIT(page->page_map[index],
														  bit);
                		page->ref_count -= 1;
                		if (page->page_full) page->page_full = FALSE;
                		if (page->ref_count == 0 && page->is_malloced == TRUE)
                		{
                    		prev->next = page->next;
                    		free(page);
                		}
                		else
                		{
                        	header->debug_depth = 0;
                        	if (header < page->first_free_header
                         	|| page->first_free_header == NULL)
                        	{
                            	page->offset = offset;
                            	page->first_free_header = header;
                        	}
                    	}
                	}
            		else
            		{
                		elog(NOTICE, "FreeNodeMemory: addr %x was already free",
						     addr);
            		}
            		freed = TRUE;
        		}
        		prev = page;
    		}
		}
	}
    return(freed);
}

bool8
FreeOtherMemory(addr)

unsigned char *addr;

{
    OtherMemoryPage *page, *prev = NULL;
    OtherMemoryHeader *header;
    bool8 freed = FALSE;
    int offset;

    for (page = page_data->other_pages;
         page != NULL && !freed;
         page = page->next)
    {
        if (page->begin_addr <= addr && page->end_addr > addr)
        {
            header = (OtherMemoryHeader *) (addr - sizeof(OtherMemoryHeader));
            if (header->used)
            {
                if (!HeaderIsValid(header))
                {
                    other_memory_page_info(header);
                    elog(WARN, "FreeOtherMemory - bad header at %x",header);
                }
                header->used = FALSE;
                header->debug_depth = 0;
                page->ref_count -= 1;
                if (page->page_full) page->page_full = FALSE;
                if (header->size > page->biggest_free_object)
                    page->biggest_free_object = header->size;

                if (page->first_free_header == NULL
                 || header < page->first_free_header)
                    page->first_free_header = header;

                if (page->ref_count == 0 && page->is_malloced == TRUE)
                {
                    prev->next = page->next;
                    free(page);
                }
            }
            else
            {
                elog(NOTICE, "FreeOtherMemory: addr %x was already free", addr);
            }
            freed = TRUE;
        }
        prev = page;
    }
    return(freed);
}

bool8
FreeBigMemory(addr)

unsigned char *addr;

{
    BigMemoryPage *page, *scanner;
    bool8 freed = FALSE;
	Size size = sizeof(BigMemoryPage) - sizeof(char *);

	printf("sizeof(BigMemoryPage) is %d\n", size);

	page = (BigMemoryPage *) ((long) addr - size);

	if (PageIsValid(page))
	{
		if (page == page_data->big_pages)
		{
			page_data->big_pages = page_data->big_pages->next;
			free(page);
		}
		else
		{
			for (scanner = page_data->big_pages;
				 scanner->next != page;
				 scanner = scanner->next);

			scanner->next = page->next;
			free(page);
		}
		freed = TRUE;
	}
    return(freed);
}

/*
 ********************
 * UTILITY ROUTINES *
 ********************
 */

/*
 * Finds a free element in the page map, and sets the page map for that
 * element to one.
 */

int
SetElement(page_map)

unsigned char *page_map;

{
    int i, j;

    for (i = 0; i < MAPSIZE && page_map[i] == FULL; i++);
    for (j = 0; j < BITS_IN_BYTE && TEST_BIT(page_map[i], j); j++);
    page_map[i] = SET_BIT(page_map[i], j);
    return(BITS_IN_BYTE * i + j);
}

/*
 * LargestFreeObject returns the size of the largest free object in an
 * OtherMemoryPage.  It assumes that page->first_free_object is really
 * a free object, so it is up to the caller to make sure that this is
 * the case.
 */

Size
LargestFreeObject(page)

OtherMemoryPage *page;

{
    OtherMemoryHeader *scanner;
    Size biggest_size = 0;

    for (scanner = page->first_free_header;
         scanner <= page->last_header;
         scanner = NextOtherHeader(scanner))
    {
        if (!HeaderIsValid(scanner))
        {
            other_memory_page_info(page);
            elog(WARN, "GetOtherMemory: page corrupted!");
        }
        if (!scanner->used && scanner->size > biggest_size)
            biggest_size = scanner->size;
    }
    return(biggest_size);
}

SetNextFreeNode(page, offset)

NodeMemoryPage *page;
long offset;

{
    int major_index, minor_index, i, j;
    long free_offset;
    NodeMemoryHeader *header;

    if (page->ref_count == page->max_elements) return;

    major_index = offset / BITS_IN_BYTE;
    minor_index = offset % BITS_IN_BYTE;

    for (i = major_index; page->page_map[i] == FULL; i++);
    for (j = 0; TEST_BIT(page->page_map[i], j); j++);

    free_offset = i * BITS_IN_BYTE + j;

    header = (NodeMemoryHeader *)
             ((long)
             (page->begin_addr + free_offset
              * (page->element_size + sizeof(NodeMemoryHeader))));

    if (HeaderIsValid(header))
    {
        page->first_free_header = header;
        page->offset = free_offset;
    }
    else
    {
        elog(WARN, "SetNextFreeNode - %x bad header", header);
    }
}

/*
 ***********************
 * DEBUGGING FUNCTIONS *
 ***********************
 */

/*
 * Debugging/tracing function.  Prints a summary of the information
 * concerning an OtherMemoryPage.  
 */

int
other_memory_page_info(page)

OtherMemoryPage *page;

{
    OtherMemoryHeader *header;
    int i;

    printf("page base address is 0x%x\n", page->begin_addr);
    printf("page usage summary...\n\n");
    printf("page->ref_count is %d\n", page->ref_count);
    printf("page->total is %d\n", page->total);
    printf("page usage map is :\n\n");

    header = (OtherMemoryHeader *) page->begin_addr;
    for (i = 0; i < page->total; i++)
    {
        printf("object %d: size %d isfree %s\n", i, header->size,
            (header->used ? "false" : "true"));
        if (!HeaderIsValid(header))
            printf("header for object %d is INVALID", i);
        if (header == page->first_free_header)
            printf("page->first_free_header is set to object %d\n", i);
        header = NextOtherHeader(header);
    }
    return(0);
}

int
PushMemoryTraceLevel()

{
    if (!memory_tracing_mode) memory_tracing_mode = TRUE;
	if (trace_depth != 0xff)
	{
		trace_depth++;
		printf("PushMemoryTraceLevel(%d)\n", trace_depth);
	}
	else
		printf("PushMemoryTraceLevel: max trace levels reached.");
}

int
DumpMemoryTrace()

{
    DumpTrace(trace_depth);
}

int DumpAllTrace()

{
	DumpTrace(1);
}

bool8
FindCorruptedAddresses()

{
}

int
PopMemoryTraceLevel()
{
	if (trace_depth == 0)
	{
		printf("PopMemoryTraceLevel: not tracing\n");
	}
	else
	{
		printf("PopMemoryTraceLevel(%d)\n", trace_depth);
    	trace_depth--;
    	DecrementTraceLevel(trace_depth);
    	if (trace_depth == 0) memory_tracing_mode = FALSE;
	}
}

DumpTrace(trace_depth)

uint8 trace_depth;

{
    NodeMemoryPage *node_page;
	NodeMemoryHeader *header;
    OtherMemoryPage *other_page;
	OtherMemoryHeader *oheader;
    int i, j, old_size = 0, element_count, small_count, large_count;
    char *type;

    printf("DumpTrace Memory Summary - trace level %d \n", trace_depth);
	for (i = 0; i < number_of_nodes; i++)
	{
		if (page_data->node_table[i].used)
		{
			for (node_page = page_data->node_table[i].first_page;
				 node_page != NULL;
				 node_page = node_page->next)
			{
				element_count = 0;
				for (header = (NodeMemoryHeader *) node_page->begin_addr, j = 0;
					 j < node_page->max_elements;
					 j++,
					 header = NextNodeHeader(header, node_page->element_size))
				{
					if (header->debug_depth >= trace_depth) element_count++;
				}
				if (element_count != 0)
				{
					printf("%d nodes of the following types were allocated\n",
						   element_count);
					for (j = 0; j < page_data->node_table[i].node_count; j++)
					{
						printf("%s\n", page_data->node_table[i].names[j]);
					}
				}
			}
		}
	}

	small_count = 0;
	large_count = 0;
	for (other_page = page_data->other_pages;
		 other_page != NULL;
		 other_page = other_page->next)
	{
		for (oheader = (OtherMemoryHeader *) other_page->begin_addr;
			 HeaderIsValid(oheader);
			 oheader = NextOtherHeader(oheader))
		{
			if (oheader->debug_depth >= trace_depth)
			{
				if (oheader->size < sizeof(HeapTuple) + PALLOC_HEADER_SIZE)
				{
					large_count++;
				}
				else
				{
					small_count++;
				}
			}
		}
	}

	if (large_count == 0 && small_count == 0)
	{
		printf("No detectable non-nodes allocated.\n");
	}
	else 
	{
		if (large_count != 0)
		{
			printf("%d objects large enough to be tuples were allocated.\n",
				   large_count);
		}
		if (small_count != 0)
		{
			printf("%d small non-node objects were allocated.\n", small_count);
		}
	}
}

DecrementTraceLevel(trace_depth)

uint8 trace_depth;

{
    NodeMemoryPage *node_page;
	NodeMemoryHeader *header;
    OtherMemoryPage *other_page;
	OtherMemoryHeader *oheader;
    int i, j, old_size = 0, element_count, small_count, large_count;
    char *type;

	for (i = 0; i < number_of_nodes; i++)
	{
		if (page_data->node_table[i].used)
		{
			for (node_page = page_data->node_table[i].first_page;
				 node_page != NULL;
				 node_page = node_page->next)
			{
				element_count = 0;
				for (header = (NodeMemoryHeader *) node_page->begin_addr, j = 0;
					 j < node_page->max_elements;
					 j++,
					 header = NextNodeHeader(header, node_page->element_size))
				{
					if (header->debug_depth > trace_depth)
						header->debug_depth = trace_depth;
				}
			}
		}
	}

	for (other_page = page_data->other_pages;
		 other_page != NULL;
		 other_page = other_page->next)
	{
		for (oheader = (OtherMemoryHeader *) other_page->begin_addr;
			 HeaderIsValid(oheader);
			 oheader = NextOtherHeader(oheader))
		{
			if (oheader->debug_depth > trace_depth)
				oheader->debug_depth = trace_depth;
		}
	}
}
