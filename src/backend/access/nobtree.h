/*
 *  btree.h -- header file for postgres btree access method implementation.
 *
 *	$Header$
 */

/*
 *  NONOBTPageOpaqueData -- At the end of every page, we store a pointer to
 *  both siblings in the tree.  See Lehman and Yao's paper for more info.
 *  In addition, we need to know what sort of page this is (leaf or internal),
 *  and whether the page is available for reuse.
 *
 *  Lehman and Yao's algorithm requires a ``high key'' on every page.  The
 *  high key on a page is guaranteed to be greater than or equal to any key
 *  that appears on this page.  Our insertion algorithm guarantees that we
 *  can use the initial least key on our right sibling as the high key.  We
 *  allocate space for the line pointer to the high key in the opaque data
 *  at the end of the page.
 *
 *  Rightmost pages in the tree have no high key.
 */

typedef struct NONOBTPageOpaqueData {
	PageNumber	nobtpo_prev;
	PageNumber	nobtpo_next;
	uint16		nobtpo_flags;

#define NOBTP_LEAF	(1 << 0)
#define NOBTP_ROOT	(1 << 1)
#define NOBTP_FREE	(1 << 2)

} NONOBTPageOpaqueData;

typedef NONOBTPageOpaqueData	*NOBTPageOpaque;

/*
 *  ScanOpaqueData is used to remember which buffers we're currently
 *  examining in the scan.  We keep these buffers locked and pinned and
 *  recorded in the opaque entry of the scan in order to avoid doing a
 *  ReadBuffer() for every tuple in the index.  This avoids semop() calls,
 *  which are expensive.
 */

typedef struct NONOBTScanOpaqueData {
    Buffer	nobtso_curbuf;
    Buffer	nobtso_mrkbuf;
} NONOBTScanOpaqueData;

typedef NONOBTScanOpaqueData	*NOBTScanOpaque;

/*
 *  NOBTItems are what we store in the btree.  Each item has an index tuple,
 *  including key and pointer values.  In addition, we must guarantee that
 *  all tuples in the index are unique, in order to satisfy some assumptions
 *  in Lehman and Yao.  The way that we do this is by storing the XID of the
 *  inserting transaction, and a sequence number, which tells us which index
 *  tuple insertion in that transaction this is.  With alignment, this is
 *  twelve bytes of space overhead in order to provide high concurrency and
 *  short-duration locks.
 */

typedef struct NONOBTItemData {
	TransactionIdData	nobti_xid;
	uint32			nobti_seqno;
	IndexTupleData		nobti_itup;
} NONOBTItemData;

typedef NONOBTItemData	*NOBTItem;

/*
 *  The following macros manage btitem sequence numbers for insertions.
 *  The global variable is in private space, so we won't have contention
 *  problems since we further disambiguate with XID.  However, this does
 *  mean that this algorithm is not easy to parallelize.
 */

extern uint32 NOBTCurrSeqNo;
#define	NOBTSEQ_INIT()	NOBTCurrSeqNo = 0
#define NOBTSEQ_GET()	NOBTCurrSeqNo++

/*
 *  NONOBTStackData -- As we descend a tree, we push the (key, pointer) pairs
 *  from internal nodes onto a private stack.  If we split a leaf, we use
 *  this stack to walk back up the tree and insert data into parent nodes
 *  (and possibly to split them, too).  Lehman and Yao's update algorithm
 *  guarantees that under no circumstances can our private stack give us
 *  an irredeemably bad picture up the tree.  Again, see the paper for
 *  details.
 */

typedef struct NONOBTStackData {
	PageNumber		nobts_blkno;
	OffsetIndex		nobts_offset;
	NOBTItem			nobts_btitem;
	struct NONOBTStackData	*nobts_parent;
} NONOBTStackData;

typedef NONOBTStackData	*NOBTStack;

/*
 *  We need to be able to tell the difference between read and write
 *  requests for pages, in order to do locking correctly.
 */

#define	NOBT_READ		0
#define	NOBT_WRITE	1

/*
 *  Similarly, the difference between insertion and non-insertion binary
 *  searches on a given page makes a difference when we're descending the
 *  tree.
 */

#define NOBT_INSERTION	0
#define NOBT_DESCENT	1

/*
 *  In general, the btree code tries to localize its knowledge about page
 *  layout to a couple of routines.  However, we need a special value to
 *  indicate "no page number" in those places where we expect page numbers.
 */

#define P_NONE		0

/*
 *  Strategy numbers -- ordering of these is <, <=, =, >=, > 
 */

#define NOBTLessStrategyNumber		1
#define NOBTLessEqualStrategyNumber	2
#define NOBTEqualStrategyNumber		3
#define NOBTGreaterEqualStrategyNumber	4
#define NOBTGreaterStrategyNumber		5
#define NOBTMaxStrategyNumber		5

/*
 *  When a new operator class is declared, we require that the user supply
 *  us with an amproc procudure for determining whether, for two keys a and
 *  b, a < b, a = b, or a > b.  This routine must return < 0, 0, > 0,
 *  respectively, in these three cases.  Since we only have one such proc
 *  in amproc, it's number 1.
 */

#define NOBTORDER_PROC	1

/* public routines */
char			*nobtgettuple();
InsertIndexResult	nobtinsert();

/* private routines */
InsertIndexResult	_nobt_doinsert();
InsertIndexResult	_nobt_insertonpg();
OffsetIndex		_nobt_pgaddtup();
ScanKey			_nobt_mkscankey();
NOBTStack			_nobt_search();
NOBTStack			_nobt_searchr();
void			_nobt_relbuf();
Buffer			_nobt_getbuf();
void			_nobt_wrtbuf();
void			_nobt_wrtnorelbuf();
bool			_nobt_skeycmp();
OffsetIndex		_nobt_binsrch();
OffsetIndex		_nobt_firsteq();
Buffer			_nobt_getstackbuf();
RetrieveIndexResult	_nobt_first();
RetrieveIndexResult	_nobt_next();
RetrieveIndexResult	_nobt_endpoint();
bool			_nobt_step();
bool			_nobt_twostep();
StrategyNumber		_nobt_getstrat();
bool			_nobt_invokestrat();
NOBTItem			_nobt_formitem();
bool			_nobt_goesonpg();
void			_nobt_regscan();
void			_nobt_dropscan();
void			_nobt_adjscans();
void			_nobt_scandel();
bool			_nobt_scantouched();
