/* ----------------------------------------------------------------
 * btree.h --
 *	B-tree definitions.
 *
 * Note:
 *	Based on Philip L. Lehman and S. Bing Yao's paper, "Efficient
 *	Locking for Concurrent Operations on B-trees," ACM Transactions
 *	on Database Systems, v.6, n.4, Dec. '81, pp650-670.
 *
 * Identification:
 *	$Header$
 * ----------------------------------------------------------------
 */

#ifndef	BTreeIncluded	/* Include this file only once. */
#define BTreeIncluded	1

/* #define DebuggingBtreeStuff 1 */

/* ----------------------------------------------------------------
 *	btree access method include files
 * ----------------------------------------------------------------
 */

#ifndef C_H
#include "c.h"
#endif

#include "align.h"
#include "attnum.h"
#include "attval.h"
#include "block.h"
#include "buf.h"
#include "bufmgr.h"
#include "bufpage.h"
#include "clib.h"
#include "datum.h"
#include "form.h"
#include "ftup.h"
#include "genam.h"
#include "heapam.h"
#include "ibit.h"
#include "imark.h"
#include "iqual.h"
#include "isop.h"
#include "istrat.h"
#include "item.h"
#include "itemid.h"
#include "itemptr.h"
#include "itup.h"
#include "log.h"
#include "misc.h"
#include "name.h"
#include "page.h"
#include "pagenum.h"
#include "part.h"
#include "postgres.h"
#include "regproc.h"
#include "rel.h"
#include "relscan.h"
#include "rlock.h"
#include "sdir.h"
#include "skey.h"
#include "tqual.h"

/* ----------------------------------------------------------------
 *	B-Tree constants
 * ----------------------------------------------------------------
 */

/* ----------------
 * BTREEISUNORDERED --
 *	Set iff the B-tree should be treated as an unordered index.
 * ----------------
 */

#define	BTREEISUNORDERED	0

#define BTreeRootBlockNumber	((BlockNumber) 0)
#define BTreeRootPageNumber	((PageNumber) 0)
#define FirstOffsetNumber	1

#define BTreeDefaultPageSize	(BLCKSZ)

extern  Name			BTreeAccessMethodName;
extern	ItemPointer		BTreeRootItemPointer;
extern	PagePartition		BTreeDefaultPagePartition;

#define BTreeNumberOfStrategies			5

#define BTreeLessThanStrategyNumber		((StrategyNumber) 1)
#define BTreeLessThanOrEqualStrategyNumber	((StrategyNumber) 2)
#define BTreeEqualStrategyNumber		((StrategyNumber) 3)
#define BTreeGreaterThanOrEqualStrategyNumber	((StrategyNumber) 4)
#define BTreeGreaterThanStrategyNumber		((StrategyNumber) 5)

#define EmptyPageOffsetIndex	((OffsetIndex) -1)

/* ----------------------------------------------------------------
 *	BTreeHeaders form the "link" information in the btree
 *	nodes. They are stored in the "Special" space on a page.
 * ----------------------------------------------------------------
 */

/* ----------------
 *	btree header structure definitions
 * ----------------
 * Note:
 *	It might be better to treat the first page of a block
 *	specially by adding a bitmap of the used pages!?!
 * ----------------
 */

/* ----------------
 * 	BTreeHeaderData
 * ----------------
 */

typedef bits8 BTreeHeaderTypeData;

#define BTREE_PAGE_IS_FREE	((BTreeHeaderTypeData) 0)
#define BTREE_INTERNAL_HEADER	((BTreeHeaderTypeData) 1)
#define BTREE_LEAF_HEADER	((BTreeHeaderTypeData) 2)

typedef struct BTreeHeaderData {
   BTreeHeaderTypeData	type;
   ItemPointerData	linkData;
   ItemPointerData	prevData;
} BTreeHeaderData;

typedef BTreeHeaderData *BTreeHeader;

#define BTreeHeaderSize	sizeof(BTreeHeaderData)




/* ----------------------------------------------------------------
 *	BTreeNodes are the in-memory representations of
 *	the nodes in the btree. They reference pages on disk
 *	which contain the actual btree information.
 * ----------------------------------------------------------------
 */

/* ----------------
 * BTreeNodeData --
 *	B-tree node information.
 *
 * Note:
 *	blockNumber and partition are computable from Buffer.
 * ----------------
 */
typedef struct BTreeNodeData {
        Relation	    relation;
	Buffer		    buffer;
	BlockNumber	    blockNumber;
	PageNumber	    pageNumber;
	PagePartition	    partition;
	BTreeHeaderTypeData type;
} BTreeNodeData;

typedef BTreeNodeData	*BTreeNode;

#define InvalidBTreeNode	NULL


/* ----------------------------------------------------------------
 *	BTreeItemPointerData
 * ----------------------------------------------------------------
 */

typedef struct BTreeItemPointerData {
   BTreeNode 	node;
   OffsetNumber offsetNumber;
} BTreeItemPointerData;

typedef struct BTreeItemPointerData *BTreeItemPointer;

/* ----------------------------------------------------------------
 *	BTreeItems form the (key, ptr) tuples stored on the
 *	page. 
 * ----------------------------------------------------------------
 */

/* ----------------
 *	BTreeItemFlags are used to classify btree items
 * ----------------
 */

typedef bits16 BTreeItemFlags;

#define	BTREE_ITEM_IS_LEAF	((BTreeItemFlags) (1 << 0))
#define	BTREE_ITEM_IS_HIGHKEY	((BTreeItemFlags) (1 << 1))

/* ----------------
 * BTreeItem --
 *	Either a pointer to an internal or leaf B-tree node item.
 * ----------------
 */

typedef struct BTreeItemHeaderData {
   BTreeItemFlags		flags;
   IndexTupleData		ituple;
} BTreeItemHeaderData;

typedef struct BTreeItemData {
   BTreeItemHeaderData		header;
   FormData			form;
       /* VARIABLE LENGTH STRUCTURE */
} BTreeItemData;

typedef BTreeItemData *BTreeItem;






/* ----------------------------------------------------------------
 *	BTreeData, BTreeInsertData, BTreeSearchKeys and
 *	ItemPointerElements are internal structures used during
 *	searching for and insertion of btree items.
 * ----------------------------------------------------------------
 */

/* ----------------
 *	BTreeData definition
 * ----------------
 */

typedef struct BTreeDataData {
   ItemPointer	pointer;
   RuleLock	lock;
} BTreeDataData;

typedef BTreeDataData	*BTreeData;

/* ----------------
 *	BTreeInsertData definition
 * ----------------
 */

#define BTREE_INTERNAL_INSERT_DATA 1
#define BTREE_LEAF_INSERT_DATA 2

typedef struct BTreeInsertDataData {
   int8			type;			/* internal or leaf */
   
   /* data necessary to form both internal and leaf btree items */
   IndexTuple		indexTuple;
   Size			size;

   /* data specific to leaf insertions */
   ItemPointerData	leafPointerData; 	/* return */
   RuleLock		leafLock;		/* return */ /* unused */
   double		leafOffset;		/* return */
} BTreeInsertDataData;

typedef BTreeInsertDataData	*BTreeInsertData;


/* ----------------
 * 	BTreeSearchKey definition
 *
 *	Note:
 *	originalInsertData is a redundant pointer to the initial
 *	insertData placed into the key. Because insertData may change
 *	in the case of node splitting, we keep a spare pointer to
 *	the initial insertData. This is necessary because information
 *	is returned in insertData->pointer which is used by the top
 *	level BTreeAMInsert and BTreeAMGetTuple routines.
 * ----------------
 */

typedef struct BTreeSearchKeyData {
   ItemPointer	   pointer;		/* position to start from */
   Relation	   relation;		/* index relation */
   ScanKeySize	   scanKeySize;		/* number of atts in scan key */
   ScanKey	   scanKey;		/* scan key for scans */
   BTreeInsertData insertData;		/* data to insert into a node */
   BTreeInsertData originalInsertData;	/* data to insert into leaf node */
   ScanDirection   direction;		/* forward or reverse scan */
   bool		   wasSuccessfulSearch;	/* return: was search successful? */
   bool		   isForInsertion;	/* is this an insert or a scan? */
   bool		   isStartingAtEndpoint; /* where should our scan start? */
} BTreeSearchKeyData;

typedef BTreeSearchKeyData	*BTreeSearchKey;

/* ----------------
 * 	ItemPointerElement definition
 *
 *	ItemPointerElements are used by the btree item pointer
 *	stack algorithm to implement more efficient locking.
 * ----------------
 */

typedef struct ItemPointerElementData {
	ItemPointer	pointer;
	struct ItemPointerElementData	*next;
} ItemPointerElementData;

typedef ItemPointerElementData	*ItemPointerElement;

typedef ItemPointerElement	*ItemPointerStack;

/* ----------------------------------------------------------------
 *	extern definitions
 * ----------------------------------------------------------------
 */

#include "btree-externs.h"

#endif	BTreeIncluded

