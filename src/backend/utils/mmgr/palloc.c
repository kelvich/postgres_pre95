/*
 * palloc.c --
 *	POSTGRES memory allocator code.
 */

#include "tmp/c.h"

RcsId("$Header$");

#include "utils/mcxt.h"
#include "utils/log.h"

#include "nodes/mnodes.h"

#include "utils/palloc.h"

/* ----------------------------------------------------------------
 *	User library functions
 * ----------------------------------------------------------------
 */

#undef palloc
#undef pfree
#undef MemoryContextAlloc
#undef MemoryContextFree
#undef malloc
#undef free

extern Pointer MemoryContextAlloc();
extern void MemoryContextFree();

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

SLList PallocDebugList;
SLList *PallocList = NULL;
SLList *PfreeList = NULL;
int PallocDiffTag = 0;

bool   PallocRecord;
bool   PallocNoisy;

void
set_palloc_debug(noisy, record)
    bool   noisy;
    bool   record;
{
    PallocNoisy = noisy;
    PallocRecord = record;
    SLNewList(&PallocDebugList, offsetof(PallocDebugData, Link));
}

Pointer
palloc_record(file, line, size, context)
    String file;
    int	   line;
    Size   size;
    String context;
{
    Pointer p;
    PallocDebugData *d, *d1;

    p = (Pointer) palloc(size);
    
    /* ----------------
     *	note: we have to use malloc/free for the administrative
     *        information because this has to work correctly when the
     *	      memory context stuff is not enabled.
     * ----------------
     */
    d = (PallocDebugData *) malloc(sizeof(PallocDebugData));
    
    d->pointer = p;
    d->size    = size;
    d->line    = line;
    d->file    = file;
    d->context = context;
    
    SLNewNode(&(d->Link));
    SLAddTail(&PallocDebugList, &(d->Link));
    if (PallocDiffTag) {
        d1 = (PallocDebugData *) malloc(sizeof(PallocDebugData));
        d1->pointer = p;
        d1->size    = size;
        d1->line    = line;
        d1->file    = file;
        d1->context = context;
	SLNewNode(&(d1->Link));
	SLAddTail(PallocList, &(d1->Link));
      }
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
    if (d != NULL) {
	if (PallocDiffTag)
	    SLAddTail(PfreeList, &(d->Link));
	else
	    free(d); /* can't use pfree, see palloc_record -cim  */
        if (PallocNoisy)
	    printf("!- f: %s l: %d p: 0x%x s: %d %s\n",
	           d->file, d->line, pointer, d->size, d->context);
    
    } else
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

    if (! PallocRecord)
	return;
    
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
	
    if (PallocNoisy && !PallocRecord)
	printf("!- f: %s l: %d p: 0x%x s: ? %s\n",
	       file, line, pointer, context);
}
 
void
alloc_set_message(file, line, pointer, set)
    String   file;
    int	     line;
    Pointer  pointer;
    Pointer  set;
{
    if (PallocRecord)
	pfree_remove(file, line, pointer);
}
 
void
free_palloc_list(list)
SLList *list;
{
    PallocDebugData *d;
    PallocDebugData *d1;

    d = (PallocDebugData*)SLGetHead(list);
    while (d != NULL) {
	d1 = d;
	d = (PallocDebugData*)SLGetSucc(&(d->Link));
	free(d1);
      }
    free(list);
}

void
start_palloc_list()
{
    if (PallocRecord)
        free_palloc_list(&PallocDebugList);
    SLNewList(&PallocDebugList, offsetof(PallocDebugData, Link));
    PallocRecord = 1;
}

void
print_palloc_list()
{
    PallocDebugData *d;
    int 	    i;
    Size 	    total;

    if (! PallocRecord)
	return;
    
    d = (PallocDebugData *) SLGetHead(&PallocDebugList);
    i = 0;
    total = 0;
    
    while (d != NULL) {
	printf("!! f: %s l: %d p: 0x%x s: %d %s\n",
	       d->file, d->line, d->pointer, d->size, d->context);
	i++;
	total += d->size;
	
	d = (PallocDebugData *) SLGetSucc(&(d->Link));
    }
    
    printf("\n!! print_palloc_list: items: %d totalsize: %d\n", i, total);
}

void
start_palloc_diff_list()
{
    if (PallocList)
       free_palloc_list(PallocList);
    if (PfreeList)
       free_palloc_list(PfreeList);
    PallocList = (SLList*)malloc(sizeof(SLList));
    PfreeList = (SLList*)malloc(sizeof(SLList));
    SLNewList(PallocList, offsetof(PallocDebugData, Link));
    SLNewList(PfreeList, offsetof(PallocDebugData, Link));
    PallocDiffTag = 1;
}

void
end_palloc_diff_list()
{
    if (PallocList)
       free_palloc_list(PallocList);
    if (PfreeList)
       free_palloc_list(PfreeList);
    PallocList = NULL;
    PfreeList = NULL;
    PallocDiffTag = 0;
}

void
print_palloc_diff_list()
{
    PallocDebugData *d, *d1, *tempd;
    int i, total;

    d = (PallocDebugData *) SLGetHead(PallocList);
    total = 0;
    i = 0;
    while (d != NULL) {
	printf("!! f: %s l: %d p: 0x%x s: %d %s\n",
	       d->file, d->line, d->pointer, d->size, d->context);
	i++;
	total += d->size;
	d = (PallocDebugData *) SLGetSucc(&(d->Link));
    }
    printf("\n!! new pallocs: items: %d totalsize: %d\n\n", i, total);
    d = (PallocDebugData *) SLGetHead(PfreeList);
    total = 0;
    i = 0;
    while (d != NULL) {
	printf("!! f: %s l: %d p: 0x%x s: %d %s\n",
	       d->file, d->line, d->pointer, d->size, d->context);
	i++;
	total += d->size;
	d = (PallocDebugData *) SLGetSucc(&(d->Link));
    }
    printf("\n!! new pfrees: items: %d totalsize: %d\n\n", i, total);
    total = 0;
    i = 0;
    d = (PallocDebugData *) SLGetHead(PallocList);
    while (d !=NULL) {
	d1 = (PallocDebugData *) SLGetHead(PfreeList);
	while (d1 != NULL) {
	    if (strcmp(d->file, d1->file) == 0 && d->line == d1->line &&
		d->size == d1->size) {
		SLRemove(&(d1->Link));
		free(d1);
		SLRemove(&(d->Link));
		tempd = d;
		break;
	      }
	    d1 = (PallocDebugData *) SLGetSucc(&(d1->Link));
	  }
       if (d1 == NULL) {
	   printf("!! f: %s l: %d p: 0x%x s: %d %s\n",
		  d->file, d->line, d->pointer, d->size, d->context);
	   i++;
	   total += d->size;
	 }
       d = (PallocDebugData *) SLGetSucc(&(d->Link));
       if (d1 != NULL)
	   free(tempd);
     }
    printf("\n!! unfreed allocs: items: %d totalsize: %d\n\n", i, total);
}

char *
malloc_debug(file, line, size)
String file;
int line;
int size;
{
    char *p;
    PallocDebugData *d, *d1;

    p = (char*)malloc(size);
    if (PallocRecord) {
	d = (PallocDebugData*)malloc(sizeof(PallocDebugData));
	d->pointer = (Pointer)p;
	d->size = size;
	d->line = line;
	d->file = file;
	d->context = "** Direct Malloc **";
	SLNewNode(&(d->Link));
	SLAddTail(&PallocDebugList, &(d->Link));
	if (PallocDiffTag) {
	    d1 = (PallocDebugData*)malloc(sizeof(PallocDebugData));
	    d1->pointer = p;
	    d1->size = size;
	    d1->line = line;
	    d1->file = file;
	    d1->context = "** Direct Malloc **";
	    SLNewNode(&(d1->Link));
	    SLAddTail(PallocList, &(d1->Link));
	  }
       }
    if (PallocNoisy)
	printf("!+ f: %s l: %d p: 0x%x s: %d ** Direct Malloc **\n",
	       file, line, p, size);
    return p;
}

free_debug(file, line, p)
String file;
int line;
char *p;
{
    PallocDebugData *d;

    if (PallocRecord) {
	d = (PallocDebugData*)SLGetHead(&PallocDebugList);
	while (d != NULL) {
	    if (d->pointer == (Pointer)p) {
		SLRemove(&(d->Link));
		break;
	      }
	    d = (PallocDebugData*)SLGetSucc(&(d->Link));
	  }
	if (d != NULL) {
	    if (PallocDiffTag)
		SLAddTail(PfreeList, &(d->Link));
	    else
		free(d);
	  }
	else
	    elog(NOTICE, "pfree_remove l:%d f:%s p:0x%x %s",
		 line, file, p, "** not on list **");
      }
    if (PallocNoisy)
	if (d != NULL)
	    printf("!- f: %s l: %d p: 0x%x s: %d ** Direct Malloc **\n",
	           d->file, d->line, p, d->size);
	else
	    printf("!- f: %s l: %d p: 0x%x s: ? ** Direct Malloc **\n",
	           file, line, p);
    free(p);
}
