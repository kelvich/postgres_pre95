static	char	psort_c[] = "$Header$";

#include <stdio.h>

#include "c.h"

#include "access.h"
#include "fmgr.h"
#include "log.h"
#include "psort.h"
#include "lselect.h"

#include "bufmgr.h"	/* for BLCKSZ */
#include "buf.h"
#include "fd.h"
#include "heapam.h"
#include "htup.h"
#include "rel.h"
#include "relscan.h"
#include "portal.h"	/* for {Start,End}PortalAllocMode */
#include "trange.h"

/*
 *	psort.c		- polyphase merge sort.
 *
 *	(Eventually the tape-splitting method (Knuth, Vol. III, pp281-86).)
 *
 *	Note:
 *		psort() cannot be called twice simultaneously
 *	Arguments? Variables?
 *		MAXMERGE, MAXTAPES
 */

struct	tape {
	int		tp_dummy;		/* (D) */
	int		tp_fib;			/* (A) */
	FILE		*tp_file;		/* (TAPE) */
	struct	tape	*tp_prev;
};

#define	TEMPDIR	"/tmp"

int			Nkeys;
struct	skey		*Key;
int			SortMemory;

static	int		TapeRange;		/* number of tapes - 1 (T) */
static	int		Level;			/* (l) */
static	int		TotalDummy;		/* summation of tp_dummy */
static	struct	tape	Tape[MAXTAPES];
static	long		shortzero = 0;		/* used to delimit runs */
static	struct	tuple	*LastTuple = NULL;	/* last output */

Relation		SortRdesc;		/* current tuples in memory */
struct	leftist		*Tuples;		/* current tuples in memory */

extern	FILE		*gettape();
extern	int		resettape();
extern	int		destroytape();
extern	int		initialrun();
extern	FILE		*mergeruns();
extern  bool		createrun();
/*
 *	psort		- polyphase merge sort entry point
 */
bool
psort(oldrel, newrel, nkeys, key)
Relation	oldrel, newrel;
int		nkeys;
struct	skey	key[];
{
/*	FILE		*outfile;	/* file of ordered tuples */

	TRACE(X, PDEBUG4(psort, "entering with relid", oldrel->rd_att.data[0]->attrelid, "and relid", newrel->rd_att.data[0]->attrelid));
	Nkeys = nkeys;
	Key = key;
	SortMemory = 0;
	SortRdesc = oldrel;
	StartPortalAllocMode(StaticAllocMode);	/* may not be the best place */
	initpsort();
	initialrun(oldrel);
/* call finalrun(newrel, mergerun()) instead */
	endpsort(newrel, mergeruns());
	TRACE(TR_MEMORY, PDEBUG2(psort, "memory use", SortMemory));
	EndPortalAllocMode();
	TRACE(X, PDEBUG(psort, "exiting"));
}

/*
 *	TAPENO		- number of tape in Tape
 */

#define	TAPENO(NODE)		(NODE - Tape)
#define	TUPLENO(TUP)		((TUP == NULL) ? -1 : (int) TUP->t_iid)

/*
 *	initpsort	- initializes the tapes
 *			- (polyphase merge Alg.D(D1)--Knuth, Vol.3, p.270)
 *	Returns:
 *		number of allocated tapes
 */

initpsort()
{
	register	int			i;
	register	struct	tape		*tp;

/*
	ASSERT(ntapes >= 3 && ntapes <= MAXTAPES,
	       "initpsort: Invalid number of tapes to initialize.\n");
*/

	tp = Tape;
	for (i = 0; i < MAXTAPES && (tp->tp_file = gettape()) != NULL; i++) {
		tp->tp_dummy = 1;
		tp->tp_fib = 1;
		tp->tp_prev = tp - 1;
		tp++;
	}
	TapeRange = --tp - Tape;
	tp->tp_dummy = 0;
	tp->tp_fib = 0;
	Tape[0].tp_prev = tp;

	if (TapeRange <= 1)
		elog(WARN, "initpsort: Could only allocate %d < 3 tapes\n",
		     TapeRange + 1);

	Level = 1;
	TotalDummy = TapeRange;

	SortMemory = SORTMEM;
	LastTuple = NULL;
	Tuples = NULL;
}

/*
 *	resetpsort	- resets (frees) malloc'd memory for an aborted Xaction
 *
 *	Not implemented yet.
 */

resetpsort()
{
	;
}

/*
 *	PUTTUP		- writes the next tuple
 *	ENDRUN		- mark end of run
 *	GETLEN		- reads the length of the next tuple
 *	ALLOCTUP	- returns space for the new tuple
 *	SETTUPLEN	- stores the length into the tuple
 *	GETTUP		- reads the tuple
 *
 *	Note:
 *		LEN field must be a short; FP is a stream
 */

#define	PUTTUP(TUP, FP)	fwrite((char *)TUP, (TUP)->t_len, 1, FP)
#define	ENDRUN(FP)	fwrite((char *)&shortzero, sizeof (shortzero), 1, FP)
#define	GETLEN(LEN, FP)	fread((char *)&(LEN), sizeof (shortzero), 1, FP)
#define	ALLOCTUP(LEN)	((HeapTuple)malloc((unsigned)LEN))
#define	GETTUP(TUP, LEN, FP)\
	fread((char *)(TUP) + sizeof (shortzero), (LEN) - sizeof (shortzero), 1, FP)
#define	SETTUPLEN(TUP, LEN)	(TUP)->t_len = LEN

/*
 *	USEMEM		- record use of memory
 *	FREEMEM		- record freeing of memory
 *	FULLMEM		- 1 iff a tuple will fit
 */

#define	USEMEM(AMT)	SortMemory -= (AMT)
#define	FREEMEM(AMT)	SortMemory += (AMT)
#define	LACKMEM()	(SortMemory <= BLCKSZ)		/* not accurate */
#define	TRACEMEM(FUNC)
#define	TRACEOUT(FUNC, TUP)

/*
 *	initialrun	- distributes tuples from the relation
 *			- (replacement selection(R2-R3)--Knuth, Vol.3, p.257)
 *			- (polyphase merge Alg.D(D2-D4)--Knuth, Vol.3, p.271)
 *
 *	Explaination:
 *		Tuples are distributed to the tapes as in Algorithm D.
 *		A "tuple" with t_size == 0 is used to mark the end of a run.
 *
 *	Note:
 *		The replacement selection algorithm has been modified
 *		to go from R1 directly to R3 skipping R2 the first time.
 *
 *		Maybe should use closer(rdesc) before return
 *		Perhaps should adjust the number of tapes if less than n.
 *		used--v. likely to have problems in mergeruns().
 *		Must know if should open/close files before each
 *		call to  psort()?   If should--messy??
 *
 *	Possible optimization:
 *		put the first xxx runs in quickly--problem here since
 *		I (perhaps prematurely) combined the 2 algorithms.
 *		Also, perhaps allocate tapes when needed. Split into 2 funcs.
 */

initialrun(rdesc)
	Relation	rdesc;
{
/*	register struct	tuple	*tup; */
	register struct	tape	*tp;
	HeapScanDesc	sdesc;
	int		baseruns;		/* D:(a) */
	int		morepasses;		/* EOF */

	/* XXX an appropriate time range should be provided */
	sdesc = ambeginscan(rdesc, 0, DefaultTimeRange, 0,
		(struct skey *)NULL);		/* XXX may need to pass timer */
	tp = Tape;

	if ((bool)createrun(sdesc, tp->tp_file) != false)
		morepasses = 0;
	else
		morepasses = 1 + (Tuples != NULL); 	/* (T != N) ? 2 : 1 */

	for ( ; ; ) {
		TRACE(TR_PSORT, PDEBUG(initialrun, "Run completed\n"));
		tp->tp_dummy--;
		TotalDummy--;
		if (tp->tp_dummy < (tp + 1)->tp_dummy)
			tp++;
		else if (tp->tp_dummy != 0)
			tp = Tape;
		else {
			TRACE(TR_PSORT,
			      PDEBUG2(initialrun, "Completed level", Level));
			Level++;
			baseruns = Tape[0].tp_fib;
			for (tp = Tape; tp - Tape < TapeRange; tp++) {
				TotalDummy +=
					(tp->tp_dummy = baseruns
					 + (tp + 1)->tp_fib
					 - tp->tp_fib);
				tp->tp_fib = baseruns
					+ (tp + 1)->tp_fib;
			}
			tp = Tape;			/* D4 */
		}					/* D3 */
		if (morepasses)
			if (--morepasses) {
				dumptuples(tp->tp_file);
				ENDRUN(tp->tp_file);
				continue;
			} else
				break;
		if ((bool)createrun(sdesc, tp->tp_file) == false)
			morepasses = 1 + (Tuples != NULL);
								/* D2 */
	}
	for (tp = Tape + TapeRange; tp >= Tape; tp--)
		rewind(tp->tp_file);				/* D. */
	amendscan(sdesc);
}

/*
 *	createrun	- places the next run on file
 *
 *	Uses:
 *		Tuples, which should contain any tuples for this run
 *
 *	Returns:
 *		FALSE iff process through end of relation
 *		Tuples contains the tuples for the following run upon exit
 */

bool
createrun(sdesc, file)
HeapScanDesc	sdesc;
FILE		*file;
{
	register HeapTuple	lasttuple;
	register HeapTuple	btup, tup;
	struct	leftist	*nextrun;
	Buffer	b;
	bool		foundeor;
	int		junk;
	HeapTuple	tuplecopy();

	lasttuple = NULL;
	nextrun = NULL;
	foundeor = false;
	for ( ; ; ) {
		while (LACKMEM() && Tuples != NULL) {
			if (lasttuple != NULL) {
				FREEMEM(lasttuple->t_len);
				FREE(lasttuple);
				TRACEMEM(createrun);
			}
			lasttuple = tup = gettuple(&Tuples, &junk);
			PUTTUP(tup, file);
			TRACEOUT(createrun, tup);
		}
		if (LACKMEM())
			break;
		btup = amgetnext(sdesc, NULL, &b);
		if (!HeapTupleIsValid(btup)) {
			foundeor = true;
			break;
		}
		tup = tuplecopy(btup, sdesc->rs_rd, b);
		USEMEM(tup->t_len);
		TRACEMEM(createrun);
		if (lasttuple != NULL && tuplecmp(tup, lasttuple))
			puttuple(&nextrun, tup, NULL);
		else
			puttuple(&Tuples, tup, NULL);
	}
	if (lasttuple != NULL) {
		FREEMEM(lasttuple->t_len);
		FREE(lasttuple);
		TRACEMEM(createrun);
	}
	dumptuples(file);
	ENDRUN(file);
	/* delimit the end of the run */
	Tuples = nextrun;
	return((bool)! foundeor); /* XXX - works iff bool is {0,1} */
}

/*
 *	tuplecopy	- see also tuple.c:palloctup()
 *
 *	This should eventually go there under that name?  And this will
 *	then use malloc directly (see version -r1.2).
 */

HeapTuple
tuplecopy(tup, rdesc, b)
HeapTuple	tup;
Relation	rdesc;
Buffer	b;
{
	HeapTuple	rettup;

	if (!HeapTupleIsValid(tup)) {
		return(NULL);		/* just in case */
	}
	rettup = LintCast(HeapTuple, malloc(tup->t_len));
	bcopy((char *)tup, (char *)rettup, tup->t_len);	/* XXX */
	return(rettup);
}

/*
 *	mergeruns	- merges all runs from input tapes
 *			  (polyphase merge Alg.D(D6)--Knuth, Vol.3, p271)
 *
 *	Returns:
 *		file of tuples in order
 */
FILE	*
mergeruns()
{
	register struct	tape	*tp;

	tp = Tape + TapeRange;
	merge(tp);
	rewind(tp->tp_file);
	while (--Level != 0) {
		tp = tp->tp_prev;
		rewind(tp->tp_file);
/*		resettape(tp->tp_file);		-not sufficient */
		merge(tp);
		rewind(tp->tp_file);
	}
	return(tp->tp_file);
}

/*
 *	merge		- handles a single merge of the tape
 *			  (polyphase merge Alg.D(D5)--Knuth, Vol.3, p271)
 */

merge(dest)
struct	tape	*dest;
{
	register HeapTuple	tup;
	register struct	tape	*lasttp;	/* (TAPE[P]) */
	register struct	tape	*tp;
	struct	leftist	*tuples;
	FILE		*destfile;
	int		times;		/* runs left to merge */
	int		outdummy;	/* complete dummy runs */
	short		fromtape;
	long		tuplen;
	extern	char	*malloc();

	lasttp = dest->tp_prev;
	times = lasttp->tp_fib;
	for (tp = lasttp ; tp != dest; tp = tp->tp_prev)
		tp->tp_fib -= times;
	tp->tp_fib += times;
	/* Tape[].tp_fib (A[]) is set to proper exit values */

	if (TotalDummy < TapeRange)		/* no complete dummy runs */
		outdummy = 0;
	else {
		outdummy = TotalDummy;		/* a large positive number */
		for (tp = lasttp; tp != dest; tp = tp->tp_prev)
			if (outdummy > tp->tp_dummy)
				outdummy = tp->tp_dummy;
		for (tp = lasttp; tp != dest; tp = tp->tp_prev)
			tp->tp_dummy -= outdummy;
		tp->tp_dummy += outdummy;
		TotalDummy -= outdummy * TapeRange;
		/* do not add the outdummy runs yet */
		times -= outdummy;
	}
	destfile = dest->tp_file;
	while (times-- != 0) {			/* merge one run */
		tuples = NULL;
		if (TotalDummy == 0)
			for (tp = dest->tp_prev; tp != dest; tp = tp->tp_prev) {
				GETLEN(tuplen, tp->tp_file);
				tup = ALLOCTUP(tuplen);
				USEMEM(tuplen);
				TRACEMEM(merge);
				SETTUPLEN(tup, tuplen);
				GETTUP(tup, tuplen, tp->tp_file);
				puttuple(&tuples, tup, TAPENO(tp));
			}
		else {
			for (tp = dest->tp_prev; tp != dest; tp = tp->tp_prev) {
				if (tp->tp_dummy != 0) {
					tp->tp_dummy--;
					TotalDummy--;
				} else {
					GETLEN(tuplen, tp->tp_file);
					tup = ALLOCTUP(tuplen);
					USEMEM(tuplen);
					TRACEMEM(merge);
					SETTUPLEN(tup, tuplen);
					GETTUP(tup, tuplen, tp->tp_file);
					puttuple(&tuples, tup, TAPENO(tp));
				}
			}
		}
		while (tuples != NULL) {
			/* possible optimization by using count in tuples */
			tup = gettuple(&tuples, &fromtape);
			PUTTUP(tup, destfile);
			FREEMEM(tup->t_len);
			FREE(tup);
			TRACEMEM(merge);
			GETLEN(tuplen, Tape[fromtape].tp_file);
			if (tuplen == 0)
				TRACE(TR_PSORT,
				      PDEBUG2(merge, "EOF tape", fromtape));
			else {
				tup = ALLOCTUP(tuplen);
				USEMEM(tuplen);
				TRACEMEM(merge);
				SETTUPLEN(tup, tuplen);
				GETTUP(tup, tuplen, Tape[fromtape].tp_file);
				puttuple(&tuples, tup, fromtape);
			}
		}				
		ENDRUN(destfile);
	}
	TotalDummy += outdummy;
}
		      
/*
 *	endpsort	- creates the new relation and unlinks the tape files
 */

endpsort(rdesc, file)
struct	reldesc		*rdesc;
FILE			*file;
{
	register struct	tape	*tp;
	register HeapTuple	tup;
	long		tuplen;
	char		*malloc();

	if (! feof(file))
		while (GETLEN(tuplen, file) && tuplen != 0) {
			tup = ALLOCTUP(tuplen);
			SortMemory += tuplen;
			TRACE(TR_MEMORY,
				PDEBUG2(endpsort, "memory use", SortMemory));
			SETTUPLEN(tup, tuplen);
			GETTUP(tup, tuplen, file);
			aminsert(rdesc, tup, (double *)NULL);
			FREE(tup);
			SortMemory -= tuplen;
			TRACE(TR_MEMORY,
			      PDEBUG2(endpsort, "memory use", SortMemory));
		}
	for (tp = Tape + TapeRange; tp >= Tape; tp--)
		destroytape(tp->tp_file);
}

/*
 *	gettape		- handles access temporary files in polyphase merging
 *
 *	Optimizations:
 *		If guarenteed that only one sort running/process,
 *		can simplify the file generation--and need not store the
 *		name for later unlink.
 */

struct tapelst {
	char		*tl_name;
	int		tl_fd;
	struct	tapelst	*tl_next;
};

struct	tapelst	*Tapes = NULL;
char		Tempfile[MAXPGPATH] = TEMPDIR;

/*
 *	gettape		- returns an open stream for writing/reading
 *
 *	Returns:
 *		Open stream for writing/reading.
 *		NULL if unable to open temporary file.
 */
FILE	*
gettape()
{
	register struct	tapelst	*tp;
	FILE		*file;
	static	int	tapeinit = 0;
	char		*malloc(), *mktemp();

	tp = (struct tapelst *)malloc((unsigned)sizeof (struct tapelst));
	if (!tapeinit) {
		Tempfile[sizeof (TEMPDIR) - 1] = '/';
		bcopy(TAPEEXT, Tempfile + sizeof (TEMPDIR), sizeof (TAPEEXT));
	}
	tp->tl_name = malloc((unsigned)sizeof(Tempfile));
	bcopy(Tempfile, tp->tl_name, sizeof (TEMPDIR) + sizeof (TAPEEXT) - 1);
	mktemp(tp->tl_name);

	AllocateFile();
	file = fopen(tp->tl_name, "w+");
	if (file == NULL) {
		/* XXX this should not happen */
		FreeFile();
		FREE(tp->tl_name);
		FREE(tp);
		return(NULL);
	}

	tp->tl_fd = fileno(file);
	tp->tl_next = Tapes;
	Tapes = tp;
	return(file);
}

/*
 *	resettape	- resets the tape to size 0
 */

resettape(file)
FILE		*file;
{
	register	struct	tapelst	*tp;
	register	int		fd;

	Assert(PointerIsValid(file));

	fd = fileno(file);
	for (tp = Tapes; tp != NULL && tp->tl_fd != fd; tp = tp->tl_next)
		;
	if (tp == NULL)
		elog(WARN, "resettape: tape not found");

	file = freopen(tp->tl_name, "w+", file);
	if (file == NULL) {
		elog(FATAL, "could not freopen temporary file");
	}
}

/*
 *	distroytape	- unlinks the tape
 *
 *	Efficiency note:
 *		More efficient to destroy more recently allocated tapes first.
 *
 *	Possible bugs:
 *		Exits instead of returning status, if given invalid tape.
 */

destroytape(file)
FILE		*file;
{
	register	struct	tapelst		*tp, *tq;
	register	int			fd;

	if ((tp = Tapes) == NULL)
		elog(FATAL, "destroytape: tape not found");

	if ((fd = fileno(file)) == tp->tl_fd) {
		Tapes = tp->tl_next;
		fclose(file);
		FreeFile();
		unlink(tp->tl_name);
		FREE(tp->tl_name);
		FREE(tp);
	} else
		for ( ; ; ) {
			if (tp->tl_next == NULL)
				elog(FATAL, "destroytape: tape not found");
			if (tp->tl_next->tl_fd == fd) {
				fclose(file);
				FreeFile();
				tq = tp->tl_next;
				tp->tl_next = tq->tl_next;
				unlink(tq->tl_name);
				FREE((tq->tl_name));
				FREE(tq);
				break;
			}
			tp = tp->tl_next;
		}
}
