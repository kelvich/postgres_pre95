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

#ifndef ALIGN_H
#include "align.h"
#endif
#ifndef ATTNUM_H
#include "attnum.h"
#endif
#ifndef ATTVAL_H
#include "attval.h"
#endif
#ifndef BLOCK_H
#include "block.h"
#endif
#ifndef BUF_H
#include "buf.h"
#endif
#ifndef BUFMGR_H
#include "bufmgr.h"
#endif
#ifndef BUFPAGE_H
#include "bufpage.h"
#endif
#ifndef CLIB_H
#include "clib.h"
#endif
#ifndef DATUM_H
#include "datum.h"
#endif
#ifndef FORM_H
#include "form.h"
#endif
#ifndef FTUP_H
#include "ftup.h"
#endif
#ifndef GENAM_H
#include "genam.h"
#endif
#ifndef GLOBALS_H
#include "globals.h"
#endif
#ifndef HEAPAM_H
#include "heapam.h"
#endif
#ifndef IBIT_H
#include "ibit.h"
#endif
#ifndef IMARK_H
#include "imark.h"
#endif
#ifndef IQUAL_H
#include "iqual.h"
#endif
#ifndef ISOP_H
#include "isop.h"
#endif
#ifndef ISTRAT_H
#include "istrat.h"
#endif
#ifndef ITEM_H
#include "item.h"
#endif
#ifndef ITEMID_H
#include "itemid.h"
#endif
#ifndef ITEMPTR_H
#include "itemptr.h"
#endif
#ifndef ITUP_H
#include "itup.h"
#endif
#ifndef LOG_H
#include "log.h"
#endif
#ifndef MISC_H
#include "misc.h"
#endif
#ifndef NAME_H
#include "name.h"
#endif
#ifndef PAGE_H
#include "page.h"
#endif
#ifndef PAGENUM_H
#include "pagenum.h"
#endif
#ifndef PART_H
#include "part.h"
#endif
#ifndef POSTGRES_H
#include "postgres.h"
#endif
#ifndef REGPROC_H
#include "regproc.h"
#endif
#ifndef REL_H
#include "rel.h"
#endif
#ifndef RELSCAN_H
#include "relscan.h"
#endif
#ifndef RLOCK_H
#include "rlock.h"
#endif
#ifndef SDIR_H
#include "sdir.h"
#endif
#ifndef SKEY_H
#include "skey.h"
#endif
#ifndef TQUAL_H
#include "tqual.h"
#endif

/* ----------------------------------------------------------------
 *	B-Tree constants
 * ----------------------------------------------------------------
 */

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

/*
 *  During insertion, we need to know whether we should search for the
 *  first (last) tuple matching a given key, or whether it's okay just
 *  to find any tuple.  We need to find the bounding tuple when we are
 *  inserting rule locks.
 */

#define NO_BOUND	((int8) 0)
#define LEFT_BOUND	((int8) 1)
#define RIGHT_BOUND	((int8) 2)

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
#define BTREE_INTERNAL_HEADER	((BTreeHeaderTypeData) 1<<0)
#define BTREE_LEAF_HEADER	((BTreeHeaderTypeData) 1<<1)
#define BTREE_PAGE_HAS_RULES	((BTreeHeaderTypeData) 1<<2)

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
 *
 *	RLOCK_L and RLOCK_R are used to identify rule lock marker tuples
 *	in the index.  L is the left-hand delimiter (left to right ordering
 *	in the index), and R is the right-hand.  L and R locks ALWAYS come
 *	in pairs.  No other type of rule lock appears in the index.
 * ----------------
 */

typedef bits16 BTreeItemFlags;

#define	BTREE_ITEM_IS_LEAF	((BTreeItemFlags) (1 << 0))
#define	BTREE_ITEM_IS_HIGHKEY	((BTreeItemFlags) (1 << 1))
#define	BTREE_ITEM_IS_RLOCK_L	((BTreeItemFlags) (1 << 2))
#define	BTREE_ITEM_IS_RLOCK_R	((BTreeItemFlags) (1 << 3))
#define	BTREE_ITEM_IS_RLOCK	(BTREE_ITEM_IS_RLOCK_L|BTREE_ITEM_IS_RLOCK_R)

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

/* ----------------
 * BTreeRuleLock --
 *	A marker tuple in the btree index for starting or ending a
 *	rule lock.
 *
 *	Rule locks that use the index always have some qual.  This qual
 *	is a simple (func_oid, key) pair.  When the access method is
 *	inserting new tuples, or placing rule lock tuples, it uses the
 *	func_oid to call a function comparing the "current" key (whatever
 *	that is, at the time) with the key stored in the rule qual.  It
 *	then places the new tuple appropriately based on the result.
 *
 *	Since the key value to use in calling the function manager is
 *	already stored in the BTreeItemData, we don't bother with it here.
 * ----------------
 */

typedef struct BTreeRuleLockData {
   BTreeItemHeaderData		header;
   ObjectId			func;
   FormData			form;
	/* VARIABLE LENGTH STRUCTURE */
} BTreeRuleLockData;

typedef BTreeRuleLockData *BTreeRuleLock;

/* ----------------------------------------------------------------
 *	BTreeInsertData, BTreeSearchKeys and
 *	ItemPointerElements are internal structures used during
 *	searching for and insertion of btree items.
 * ----------------------------------------------------------------
 */

/* ----------------
 *	BTreeInsertData definition
 * ----------------
 */

#define BTREE_NO_FLAGS			((int8) 0)
#define BTREE_INTERNAL_INSERT_DATA	((int8) 1<<0)
#define BTREE_LEAF_INSERT_DATA		((int8) 1<<1)
#define BTREE_RLOCK_L_INSERT_DATA	((int8) 1<<2)
#define BTREE_RLOCK_R_INSERT_DATA	((int8) 1<<3)
#define BTREE_RLOCK_INSERT_DATA		(BTREE_RLOCK_L_INSERT_DATA|BTREE_RLOCK_R_INSERT_DATA)

typedef struct BTreeInsertDataData {
   int8			type;			/* internal or leaf */
   
   /* data necessary to form both internal and leaf btree items */
   IndexTuple		indexTuple;
   Size			size;

   /* data specific to leaf insertions */
   ItemPointerData	leafPointerData; 	/* return */
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
