/*
 * palloc.c --
 *	POSTGRES memory allocator code.
 */

#include "tmp/c.h"

RcsId("$Header$");

#include "utils/mcxt.h"
#include "utils/log.h"
#include "tmp/simplelists.h"

#include "nodes/mnodes.h"

#include "utils/palloc.h"

/* ----------------------------------------------------------------
 *	User library functions
 * ----------------------------------------------------------------
 */

#undef palloc
#undef pfree

Pointer
palloc(size)
    Size	size;
{
    return (MemoryContextAlloc(CurrentMemoryContext, size));
}

void
pfree(pointer)
    Pointer	pointer;
{	
    MemoryContextFree(CurrentMemoryContext, pointer);
}

Size
psize(pointer)
    Pointer	pointer;
{
    return (PointerGetAllocSize(pointer));
}

Pointer
repalloc(pointer, size)
    Pointer	pointer;
    Size	size;
{
    return (MemoryContextRealloc(CurrentMemoryContext, pointer, size));
}

/* ----------------------------------------------------------------
 *	palloc_debug() and pfree_debug() support routines
 * ----------------------------------------------------------------
 */

String
pcontext()
{
    return (String)
	MemoryContextGetName(CurrentMemoryContext);
}	

typedef struct PallocDebugData {
    Pointer	pointer;
    Size	size;
    String	file;
    int		line;
    String	context;
    SLNode 	Link;
} PallocDebugData;

SLList PallocDebugList;

bool   PallocRecord;
bool   PallocNoisy;

void
set_palloc_debug(noisy, record)
    bool   noisy;
    bool   record;
{
    PallocNoisy = noisy;
    PallocRecord = record;
    SLNewList(&PallocDebugList);
}

Pointer
palloc_record(file, line, size, context)
    String file;
    int	   line;
    Size   size;
    String context;
{
    Pointer p;
    PallocDebugData *d;

    p = (Pointer) palloc(size);
    d = (PallocDebugData *) palloc(sizeof(PallocDebugData));
    SLNewNode(&(d->Link));
    
    d->pointer = p;
    d->size    = size;
    d->line    = line;
    d->file    = file;
    d->context = context;
    
    SLAddTail(&PallocDebugList, &(d->Link));
    return p;
}

void
pfree_remove(file, line, pointer)
    String  file;
    int	    line;
    Pointer pointer;
{
    PallocDebugData *d;
    
    d = (PallocDebugData *) SLGetHead(&PallocDebugList);
    while (d != NULL) {
	if (d->pointer == pointer) {
	    SLRemove(&(d->Link));
	    break;
	}
	d = (PallocDebugData *) SLGetSucc(&(d->Link));
    }
    
    /* ----------------
     *	at this point, if d is null it means we couldn't find
     *  the pointer on the allocation list and we are accidentally
     *  freeing something we shouldn't.  Otherwise, it means we
     *  found the pointer so we can free the administrative info.
     * ----------------
     */
    if (d != NULL)
	pfree(d);
    else
	elog(NOTICE, "pfree_remove l:%d f:%s p:0x%x %s",
	     line, file, pointer, "** not on list **");
    
}

void
pfree_record(file, line, pointer)
    String  file;
    int	    line;
    Pointer pointer;
{
    /* ----------------
     *  scan our allocation list for the appropriate debug entry.
     *  when we find the entry in the list, remove it.  then pfree()
     *  the pointer itself.
     * ----------------
     */
    pfree_remove(file, line, pointer);
    pfree(pointer);
}

void
dump_palloc_list(caller, verbose)
    String caller;
    bool   verbose;
{
    PallocDebugData *d;
    int 	    i;
    Size 	    total;
    
    d = (PallocDebugData *) SLGetHead(&PallocDebugList);
    i = 0;
    total = 0;
    
    while (d != NULL) {
	if (verbose)
	    printf("!! f: %s l: %d p: 0x%x s: %d %s\n",
		   d->file, d->line, d->pointer, d->size, d->context);
	i++;
	total += d->size;
	
	d = (PallocDebugData *) SLGetSucc(&(d->Link));
    }
    
    printf("\n!! dump_palloc_list: %s items: %d totalsize: %d\n",
	   caller, i, total);
}

/* ----------------------------------------------------------------
 *	palloc_debug() and pfree_debug()
 * ----------------------------------------------------------------
 */

Pointer
palloc_debug(file, line, size)
    String file;
    int	   line;
    Size   size;
{
    Pointer p;
    String context = pcontext();
    
    if (PallocRecord)
	p = palloc_record(file, line, size, context);
    else
	p = palloc(size);
	
    if (PallocNoisy)
	printf("!+ f: %s l: %d p: 0x%x s: %d %s\n",
	       file, line, p, size, context);
    
    return p;
}
 
void
pfree_debug(file, line, pointer)
    String  file;
    int	    line;
    Pointer pointer;
{
    String context = pcontext();
    
    if (PallocRecord)
	pfree_record(file, line, pointer);
    else
	pfree(pointer);
	
    if (PallocNoisy)
	printf("!- f: %s l: %d p: 0x%x %s\n",
	       file, line, pointer, context);
}
 
void
alloc_set_message(file, line, pointer, set)
    String   file;
    int	     line;
    Pointer  pointer;
    Pointer  set;
{
    if (PallocNoisy)
	printf("!* f: %s l: %d p: 0x%x s:0x%x \n",
	       file, line, pointer, set);

    if (PallocRecord)
	pfree_remove(file, line, pointer);
}
 
