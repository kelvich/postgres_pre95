#ifndef lint
static char order_buf_c[] =
	"$Header$";
#endif

/*
 *	Routines to order the writing of buffers to disk.
 */

#include "internal.h"
#include "log.h"
#include "postgres.h"


static Stack		*adjlist[NDBUFS];
static Lbufdesc 	*datlist[NDBUFS];
static int		used[NDBUFS];


/*
 *	push, pop
 *
 *	Routines to implement a simple stack.
 */

static
push(i, s)
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
pop(s)
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

	for (i = 0; i < NDBUFS; i++) {
		adjlist[i] = (Stack *) NULL;
		datlist[i] = (Lbufdesc *) NULL;
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
	Lbufdesc 	*predb, *succb;
{
	register int	i;
	int		before = -1, after = -1;

	if (predb == succb) {
		elog(DEBUG, "BufferWriteInOrder: predb == succb!  Ignoring...");
		return(0);
	}

	/* Save the buffer pointers in our static array */
	for (i = 0; i < NDBUFS; i++) {
		if (datlist[i] == (Lbufdesc *) NULL)
			break;
		if (datlist[i] == predb)
			before = i;
		if (datlist[i] == succb)
			after = i;
	}
	if (before == -1 && i < NDBUFS) {
		datlist[i] = predb;
		before = i++;
	} else if (i == NDBUFS) {
		elog(WARN, "BufferWriteInOrder: weirdness with predb");
		return(-1);
	}
	if (after == -1 && i < NDBUFS) {
		datlist[i] = succb;
		after = i;
	} else if (i == NDBUFS) {
		elog(WARN, "BufferWriteInOrder: weirdness with succb");
		return(-1);
	}
	
	/* Save the successor in the predecessor's adjacency list */
	push(after, &adjlist[before]);
	used[before] = used[after] = 1;
	return(0);
}

/*
 *	bwsort
 *
 *	Determines which buffers must be written before 'item', given the
 *	partial order (DAG) created by calls to bw_before().
 *
 *	Returns an array of size NDBUFS which contains the buffer numbers
 *	in the order in which they must be written (up to 'item').
 *
 *	Cannot cope with cycles.
 *
 *	Uses the O(n+e) topological sort from Mellhorn, Vol. I.
 */

Lbufdesc **
bwsort(item)
	Lbufdesc	*item;
{
	register int		i;
	int			count = 0, active_bufs = 0;
	register Stack		*sp;
	Stack			*zeroindeg = (Stack *) NULL;
	int			indeg[NDBUFS];
	static Lbufdesc	*ret[NDBUFS];


	for (i = 0; i < NDBUFS; i++) {
		ret[i] = (Lbufdesc *) NULL;
		indeg[i] = 0;
	}

	/* Calculate indegrees for each vertex */
	for (i = 0; i < NDBUFS; i++)
		for (sp = adjlist[i]; sp != (Stack *) NULL; sp = sp->next)
			indeg[sp->datum]++;

	/*
	 * Push all 'root' vertices onto the stack.
	 * While we do this, check to see if 'item' has an indegree of 0.
	 * 	If so, we don't have to do the topological sort.
	 */
	for (i = 0; i < NDBUFS; i++)
		if (indeg[i] == 0)
			if (datlist[i] == item) {	/* no need to sort */
				ret[0] = item;
				return(ret);
			} else
				push(i, &zeroindeg);

	/*
	 * Perform the topological sort.
	 */
	while (zeroindeg != (Stack *) NULL) {
		if ((i = pop(&zeroindeg)) < 0) {
			elog(WARN, "bwsort: stack error");
			return((Lbufdesc **) NULL);
		}
		if (used[i])			/* ignore unused buffers */
			ret[count++] = datlist[i];
		for (sp = adjlist[i]; sp != (Stack *) NULL; sp = sp->next) {
			indeg[sp->datum]--;
			if (indeg[sp->datum] == 0)
				push(sp->datum, &zeroindeg);
		}
	}

	/* Either 'item' is last or we have some kind of error */
	for (i = 0; i < NDBUFS; i++)
		active_bufs += used[i];
	if (count != active_bufs) {
		elog(WARN, "bwsort: cycle in buffer ordering!");
		return((Lbufdesc **) NULL);
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
	Lbufdesc	*items[];
{
	register Stack	*p;
	register int	i, j;
	int		remove[NDBUFS];

	if (nitems < 0 || nitems > NDBUFS) {
		elog(WARN, "bwremove: bad nitems %d", nitems);
		return(-1);
	}

	/* Mark off the items in 'datlist' to remove */
	for (i = 0; i < nitems; i++) 
		for (j = 0; j < NDBUFS; j++)
			if (items[i] == datlist[j])
				remove[j] = 1;
	for (i = 0; i < nitems; i++)
		if (remove[i] != 1) {
			elog(WARN, "bwremove: ordering corrupted");
			return(-1);
		}

	/* Remove everything in 'adjlist' having to do with those items */
	for (i = 0; i < NDBUFS; i++) {
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
