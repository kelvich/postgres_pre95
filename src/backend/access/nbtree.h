/*
 *  btree.h -- header file for postgres btree access method implementation.
 *
 *	$Header$
 */

/*
 *  BTPageOpaqueData -- At the end of every page, we store a pointer to
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

typedef struct BTPageOpaqueData {
	PageNumber	btpo_prev;
	PageNumber	btpo_next;
	uint16		btpo_flags;

#define BTP_LEAF	(1 << 0)
#define BTP_ROOT	(1 << 1)
#define BTP_FREE	(1 << 2)

} BTPageOpaqueData;

typedef BTPageOpaqueData	*BTPageOpaque;

/*
 *  ScanOpaqueData is used to remember which buffers we're currently
 *  examining in the scan.  We keep these buffers locked and pinned and
 *  recorded in the opaque entry of the scan in order to avoid doing a
 *  ReadBuffer() for every tuple in the index.  This avoids semop() calls,
 *  which are expensive.
 */

typedef struct BTScanOpaqueData {
    Buffer	btso_curbuf;
    Buffer	btso_mrkbuf;
} BTScanOpaqueData;

typedef BTScanOpaqueData	*BTScanOpaque;

/*
 *  BTItems are what we store in the btree.  Each item has an index tuple,
 *  including key and pointer values.  In addition, we must guarantee that
 *  all tuples in the index are unique, in order to satisfy some assumptions
 *  in Lehman and Yao.  The way that we do this is by storing the XID of the
 *  inserting transaction, and a sequence number, which tells us which index
 *  tuple insertion in that transaction this is.  With alignment, this is
 *  twelve bytes of space overhead in order to provide high concurrency and
 *  short-duration locks.
 */

typedef struct BTItemData {
	TransactionIdData	bti_xid;
	uint32			bti_seqno;
	IndexTupleData		bti_itup;
} BTItemData;

typedef BTItemData	*BTItem;

/*
 *  The following macros manage btitem sequence numbers for insertions.
 *  The global variable is in private space, so we won't have contention
 *  problems since we further disambiguate with XID.  However, this does
 *  mean that this algorithm is not easy to parallelize.
 */

extern uint32 BTCurrSeqNo;
#define	BTSEQ_INIT()	BTCurrSeqNo = 0
#define BTSEQ_GET()	BTCurrSeqNo++

/*
 *  BTStackData -- As we descend a tree, we push the (key, pointer) pairs
 *  from internal nodes onto a private stack.  If we split a leaf, we use
 *  this stack to walk back up the tree and insert data into parent nodes
 *  (and possibly to split them, too).  Lehman and Yao's update algorithm
 *  guarantees that under no circumstances can our private stack give us
 *  an irredeemably bad picture up the tree.  Again, see the paper for
 *  details.
 */

typedef struct BTStackData {
	PageNumber		bts_blkno;
	OffsetIndex		bts_offset;
	BTItem			bts_btitem;
	struct BTStackData	*bts_parent;
} BTStackData;

typedef BTStackData	*BTStack;

/*
 *  We need to be able to tell the difference between read and write
 *  requests for pages, in order to do locking correctly.
 */

#define	BT_READ		0
#define	BT_WRITE	1

/*
 *  Similarly, the difference between insertion and non-insertion binary
 *  searches on a given page makes a difference when we're descending the
 *  tree.
 */

#define BT_INSERTION	0
#define BT_DESCENT	1

/*
 *  In general, the btree code tries to localize its knowledge about page
 *  layout to a couple of routines.  However, we need a special value to
 *  indicate "no page number" in those places where we expect page numbers.
 */

#define P_NONE		0

/*
 *  Strategy numbers -- ordering of these is <, <=, =, >=, > 
 */

#define BTLessStrategyNumber		1
#define BTLessEqualStrategyNumber	2
#define BTEqualStrategyNumber		3
#define BTGreaterEqualStrategyNumber	4
#define BTGreaterStrategyNumber		5
#define BTMaxStrategyNumber		5

/*
 *  When a new operator class is declared, we require that the user supply
 *  us with an amproc procudure for determining whether, for two keys a and
 *  b, a < b, a = b, or a > b.  This routine must return < 0, 0, > 0,
 *  respectively, in these three cases.  Since we only have one such proc
 *  in amproc, it's number 1.
 */

#define BTORDER_PROC	1

/* public routines */
char			*btgettuple();
InsertIndexResult	btinsert();

/* private routines */
InsertIndexResult	_bt_doinsert();
InsertIndexResult	_bt_insertonpg();
OffsetIndex		_bt_pgaddtup();
ScanKey			_bt_mkscankey();
BTStack			_bt_search();
BTStack			_bt_searchr();
void			_bt_relbuf();
Buffer			_bt_getbuf();
void			_bt_wrtbuf();
void			_bt_wrtnorelbuf();
bool			_bt_skeycmp();
OffsetIndex		_bt_binsrch();
OffsetIndex		_bt_firsteq();
Buffer			_bt_getstackbuf();
RetrieveIndexResult	_bt_first();
RetrieveIndexResult	_bt_next();
RetrieveIndexResult	_bt_endpoint();
bool			_bt_step();
bool			_bt_twostep();
StrategyNumber		_bt_getstrat();
bool			_bt_invokestrat();
BTItem			_bt_formitem();
bool			_bt_goesonpg();
void			_bt_regscan();
void			_bt_dropscan();
void			_bt_adjscans();
void			_bt_scandel();
bool			_bt_scantouched();
