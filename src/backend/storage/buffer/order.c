/*
 * order.c --
 *	Routines to order the writing of buffers to disk.
 */

#include "tmp/c.h"

RcsId("$Header$");

#include "utils/log.h"

#include "internal.h"

extern char *calloc();
extern int NBuffers;
#define ALLOC(t, c)	(t *)calloc((unsigned)(c), sizeof(t))

static Stack		**adjlist;
static BufferDesc 	**datlist;
static int		*used;


/*
 *	bs_push, bs_pop
 *
 *	bs_ stands for buffer stack; there is a routine elsewhere in the
 *	system called push(), so these have been renamed.
 *
 *	Routines to implement a simple stack.
 */

static
bs_push(i, s)
	int	i;
	Stack	**s;
{
	Stack	*sp;

	sp = ALLOC(Stack, 1);
	sp->datum = i;
	sp->next = *s;
	*s = sp;
}	

static
bs_pop(s)
	Stack	**s;
{
	extern	free();
	Stack	*sp;
	int	ret;

	if (s == (Stack **) NULL)
		return(-1);
	if (*s == (Stack *) NULL)
		return(-1);

	ret = (*s)->datum;
	sp = (*s)->next;
	free((char *) *s);
	*s = sp;
	return(ret);
}


/*
 *	bwinit
 *
 *	Initialize the data structures needed for buffer ordering.
 */

bwinit()
{
	register int	i;

	adjlist = (Stack**)malloc(NBuffers * sizeof(Stack*));
	datlist = (BufferDesc**)malloc(NBuffers * sizeof(BufferDesc*));
	used = (int*)malloc(NBuffers * sizeof(int));

	for (i = 0; i < NBuffers; i++) {
		adjlist[i] = (Stack *) NULL;
		datlist[i] = (BufferDesc *) NULL;
		used[i] = 0;
	}
}


/*
 *	bworder
 *
 *	Enter two buffers into the partial ordering.
 *
 *	Returns -1 if an error occurs, 0 if not.
 */

BufferWriteInOrder(predb, succb)
	BufferDesc 	*predb, *succb;
{
	register int	i;
	int		before = -1, after = -1;

	if (predb == succb) {
		elog(DEBUG, "BufferWriteInOrder: predb == succb!  Ignoring...");
		return(0);
	}

	/* Save the buffer pointers in our static array */
	for (i = 0; i < NBuffers; i++) {
		if (datlist[i] == (BufferDesc *) NULL)
			break;
		if (datlist[i] == predb)
			before = i;
		if (datlist[i] == succb)
			after = i;
	}
	if (before == -1 && i < NBuffers) {
		datlist[i] = predb;
		before = i++;
	} else if (i == NBuffers) {
		elog(WARN, "BufferWriteInOrder: weirdness with predb");
		return(-1);
	}
	if (after == -1 && i < NBuffers) {
		datlist[i] = succb;
		after = i;
	} else if (i == NBuffers) {
		elog(WARN, "BufferWriteInOrder: weirdness with succb");
		return(-1);
	}
	
	/* Save the successor in the predecessor's adjacency list */
	bs_push(after, &adjlist[before]);
	used[before] = used[after] = 1;
	return(0);
}

/*
 *	bwsort
 *
 *	Determines which buffers must be written before 'item', given the
 *	partial order (DAG) created by calls to bw_before().
 *
 *	Returns an array of size NBuffers which contains the buffer numbers
 *	in the order in which they must be written (up to 'item').
 *
 *	Cannot cope with cycles.
 *
 *	Uses the O(n+e) topological sort from Mellhorn, Vol. I.
 */

BufferDesc **
bwsort(item)
	BufferDesc	*item;
{
	register int		i;
	int			count = 0, active_bufs = 0;
	register Stack		*sp;
	Stack			*zeroindeg = (Stack *) NULL;
	int			*indeg;
	static BufferDesc	**ret;


	indeg = (int*)alloca(NBuffers * sizeof(int));
	ret = (BufferDesc**)alloca(NBuffers * sizeof(BufferDesc*));
	for (i = 0; i < NBuffers; i++) {
		ret[i] = (BufferDesc *) NULL;
		indeg[i] = 0;
	}

	/* Calculate indegrees for each vertex */
	for (i = 0; i < NBuffers; i++)
		for (sp = adjlist[i]; sp != (Stack *) NULL; sp = sp->next)
			indeg[sp->datum]++;

	/*
	 * Push all 'root' vertices onto the stack.
	 * While we do this, check to see if 'item' has an indegree of 0.
	 * 	If so, we don't have to do the topological sort.
	 */
	for (i = 0; i < NBuffers; i++)
		if (indeg[i] == 0)
			if (datlist[i] == item) {	/* no need to sort */
				ret[0] = item;
				return(ret);
			} else
				bs_push(i, &zeroindeg);

	/*
	 * Perform the topological sort.
	 */
	while (zeroindeg != (Stack *) NULL) {
		if ((i = bs_pop(&zeroindeg)) < 0) {
			elog(WARN, "bwsort: stack error");
			return((BufferDesc **) NULL);
		}
		if (used[i])			/* ignore unused buffers */
			ret[count++] = datlist[i];
		for (sp = adjlist[i]; sp != (Stack *) NULL; sp = sp->next) {
			indeg[sp->datum]--;
			if (indeg[sp->datum] == 0)
				bs_push(sp->datum, &zeroindeg);
		}
	}

	/* Either 'item' is last or we have some kind of error */
	for (i = 0; i < NBuffers; i++)
		active_bufs += used[i];
	if (count != active_bufs) {
		elog(WARN, "bwsort: cycle in buffer ordering!");
		return((BufferDesc **) NULL);
	}

	return(ret);
}


/*
 *	bwremove
 *
 *	Remove the argument item from the ordering data structures.
 */

bwremove(nitems, items)
	int		nitems;
	BufferDesc	*items[];
{
	register Stack	*p;
	register int	i, j;
	int		*remove;

	remove = (int*)alloca(NBuffers * sizeof(int));
	if (nitems < 0 || nitems > NBuffers) {
		elog(WARN, "bwremove: bad nitems %d", nitems);
		return(-1);
	}

	/* Mark off the items in 'datlist' to remove */
	for (i = 0; i < nitems; i++) 
		for (j = 0; j < NBuffers; j++)
			if (items[i] == datlist[j])
				remove[j] = 1;
	for (i = 0; i < nitems; i++)
		if (remove[i] != 1) {
			elog(WARN, "bwremove: ordering corrupted");
			return(-1);
		}

	/* Remove everything in 'adjlist' having to do with those items */
	for (i = 0; i < NBuffers; i++) {
		if ((p = adjlist[i]) == (Stack *) NULL)
			continue;
		else if (p->next == (Stack *) NULL)
			if (remove[p->datum])
				adjlist[i] = (Stack *) NULL;
		else
			for (; p->next != (Stack *) NULL;
			     p = p->next)
				if (remove[p->next->datum])
					p->next = p->next->next;
		used[i] = 0;
	}
	return(0);
}
