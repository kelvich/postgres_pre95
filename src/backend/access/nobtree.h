/*
 *  btree.h -- header file for postgres btree access method implementation.
 *
 *	$Header$
 */

/*
 *  NOBTPageOpaqueData -- At the end of every page, we store a pointer to
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

typedef struct NOBTPageOpaqueData {
	uint32		nobtpo_linktok;
	PageNumber	nobtpo_prev;
	PageNumber	nobtpo_oldprev;
	uint32		nobtpo_prevtok;
	PageNumber	nobtpo_next;
	PageNumber	nobtpo_oldnext;
	uint32		nobtpo_nexttok;
	uint32		nobtpo_repltok;
	PageNumber	nobtpo_replaced;
	uint16		nobtpo_flags;

#define NOBTP_LEAF	(1 << 0)
#define NOBTP_ROOT	(1 << 1)
#define NOBTP_SIBOK	(1 << 3)
#define NOBTP_FREE	(1 << 4)

} NOBTPageOpaqueData;

typedef NOBTPageOpaqueData	*NOBTPageOpaque;

/*
 *  ScanOpaqueData is used to remember which buffers we're currently
 *  examining in the scan.  We keep these buffers locked and pinned and
 *  recorded in the opaque entry of the scan in order to avoid doing a
 *  ReadBuffer() for every tuple in the index.  This avoids semop() calls,
 *  which are expensive.
 */

typedef struct NOBTScanOpaqueData {
    Buffer	nobtso_curbuf;
    Buffer	nobtso_mrkbuf;
} NOBTScanOpaqueData;

typedef NOBTScanOpaqueData	*NOBTScanOpaque;

#ifndef notdef
/*
 *  NOBTItems are what we store in the btree.  The oldchild entry is used
 *  to store a pointer to the previous child page on internal items after
 *  a new one has been added by a page split.  It's necessary to store these
 *  in order to guarantee recoverability.
 */

typedef struct NOBTItemData {
	PageNumber		nobti_oldchild;
	uint16			nobti_filler;		/* force longalign */
	IndexTupleData		nobti_itup;
} NOBTItemData;

typedef NOBTItemData	*NOBTItem;
#else /* notdef */
typedef struct NOBTIItemData {
	uint16			nobtii_flags;
	PageNumber		nobtii_oldchild;
	PageNumber		nobtii_child;
	unsigned short		nobtii_info;
	/* MORE DATA FOLLOWS AT END OF STRUCT */
} NOBTIItemData;

typedef NOBTIItemData	*NOBTIItem;

typedef struct NOBTLItemData {
	uint16			nobtli_flags;
	IndexTupleData		nobtli_itup;
	/* MORE DATA FOLLOWS AT END OF STRUCT */
} NOBTLItemData;

typedef NOBTLItemData	*NOBTLItem;
#endif /* notdef */

/*
 *  NOBTStackData -- As we descend a tree, we push the (key, pointer) pairs
 *  from internal nodes onto a private stack.  If we split a leaf, we use
 *  this stack to walk back up the tree and insert data into parent nodes
 *  (and possibly to split them, too).  Lehman and Yao's update algorithm
 *  guarantees that under no circumstances can our private stack give us
 *  an irredeemably bad picture up the tree.  Again, see the paper for
 *  details.
 *
 *  For the no-overwrite algorithm, we store the keys that bound the link
 *  we traverse in the tree in the stack.  When we reach the child, we use
 *  these keys to verify that we traversed an active link.
 */

typedef struct NOBTStackData {
	PageNumber		nobts_blkno;
	OffsetIndex		nobts_offset;
	NOBTItem		nobts_btitem;
	NOBTItem		nobts_nxtitem;
	struct NOBTStackData	*nobts_parent;
} NOBTStackData;

typedef NOBTStackData	*NOBTStack;

/*
 *  We need to be able to tell the difference between read and write
 *  requests for pages, in order to do locking correctly.
 */

#define	NOBT_READ	0
#define	NOBT_WRITE	1
#define	NOBT_NEXTLNK	2
#define	NOBT_PREVLNK	3
#define	NOBT_LINKTOK	4

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
#define NOBTGreaterStrategyNumber	5
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
NOBTStack		_nobt_search();
NOBTStack		_nobt_searchr();
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
NOBTItem		_nobt_formitem();
bool			_nobt_goesonpg();
void			_nobt_regscan();
void			_nobt_dropscan();
void			_nobt_adjscans();
void			_nobt_scandel();
bool			_nobt_scantouched();
