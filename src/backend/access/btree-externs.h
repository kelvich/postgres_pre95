/*
 * btree-externs.h --
 *	Declarations for external-interface BTree AM routines.
 *
 * Identification:
 *	$Header$
 */

#ifndef	BTreeExternsDefined
#define	BTreeExternsDefined

#ifdef NOTDEF 
/*   
   btdata.c
   ================
*/
   
extern BTreeInsertData
    IndexTupleFormInsertData();

/*   
   bthacks.c
   ================
*/
   
extern OffsetIndex    		
    PageGetActualMaxOffsetIndex();

extern void    			
    PrintBits();

extern String    		
    BTreeBufferString();

extern String    		
    BTreeItemPointerFormString();

extern void    			
    DumpBTreePage();

extern void    
    SimpleDumpBTreePage();

extern void    
    DumpBTreeNode();

extern void    
    SimpleDumpBTreeNode();

extern void    
    BTreeBlockInit();

extern bool    
    ItemKeysAreEqual();

extern void    
    BTreeInsertDataFormBTreeItem();

extern Buffer
    BTreeReadBuffer();

extern ReturnStatus
    BTreeWriteBuffer();

extern ReturnStatus
    BTreeWriteNoReleaseBuffer();

extern ReturnStatus
    BTreeReleaseBuffer();

extern void
    BTreeInitBufferPinCount();

extern void
    BTreeFreeAllPinnedBuffers();

extern bool    
    ItemIdIsBTreeRuleLock();

/*   
   bthdr.c
   ================
*/
   
extern BTreeHeader    
    PageGetBTreeHeader();

extern BTreeHeaderTypeData    
    PageGetBTreeHeaderType();

extern bool    
    PageIsLeafBTreePage();

extern ItemPointer    
    BTreeHeaderGetLinkItemPointer();

extern ItemPointer    
    BTreeHeaderGetPrevItemPointer();

extern void    
    BTreeHeaderSetLinkItemPointer();

extern void    
    BTreeHeaderSetPrevItemPointer();

/*   
   btdel.c
   ================
*/

extern void
    BTreeIndexTupleDelete();

/*   
   btinsrt.c
   ================
*/
   
extern Index    
    BTreeNodeInsertAtPosition();

extern Index    
    BTreeNodeInsert();

extern BTreeInsertData    
    BTreeRearrange();

extern void    
    BTreeAdjustRoot();

extern void    
    BTreeDataInsert();

/*   
   btitem.c
   ================
*/
   
extern IndexTuple    
    BTreeItemGetIndexTuple();

extern ItemPointer    
    BTreeItemGetHeapItemPointer();

extern ItemPointer    
    BTreeItemGetLockItemPointer();

extern ItemPointer    
    BTreeItemGetHighKeyItemPointer();

extern IndexAttributeBitMap    
    BTreeItemGetAttributeBitMap();

extern Form    
    BTreeItemGetKeyForm();

extern void    
    BTreeItemSetIsHighKey();

extern bool    
    BTreeItemTestIsHighKey();

/*   
   btkey.c
   ================
*/
   
extern bool    
    BTreeSearchKeyIsValid();

extern bool    
    BTreeSearchKeyWasSucessfulSearch();

extern BTreeSearchKey
    RelationFormBTreeSearchKey();

/*   
   btlock.c
   ================
*/
   
extern void
    BTreeLock();

extern void 
    BTreeUnlock();

/*   
   btnode.c
   ================
*/
   
extern bool    
    BTreeNodeIsLeaf();

extern bool    
    BTreeNodeIsSafe();

extern Page    
    BTreeNodeGetPage();

extern void    
    BTreeNodePutPage();

extern BTreeNode    
    BTreeNodeClear();

extern void    
    BTreeNodeCopy();

extern ItemPointer    
    BTreeNodeFormItemPointer();

extern BTreeInsertData    
    FormInternalInsertData();

extern BTreeInsertData    
    BTreeNodeFormBTreeInsertData();

extern ItemPointer    
    BTreeNodeGetLinkItemPointer();

extern void    
    BTreeNodeSetPrevItemPointer();

extern BTreeNode    
    ItemPointerGetBTreeNode();

extern void    
    ItemPointerPutBTreeNode();

extern void    
    BTreeNodeFree();

extern BTreeNode    
    RelationFormBTreeNode();

extern BTreeNode    
    RelationFormRootBTreeNode();

extern bool    
    BTreeRelationFindFreePage();

extern BTreeNode    
    CreateNewBTreeNode();

/*   
   btpage.c
   ================
*/
   
extern void    
    BTreePageCopy();

extern Index    
    BTreePageGetMaxIndex();

extern ItemPointer    
    BTreePageGetLinkItemPointer();

extern ItemPointer    
    BTreePageGetPrevItemPointer();

extern void    
    BTreePageSetLinkItemPointer();

extern void    
    BTreePageSetPrevItemPointer();

extern PageFreeSpace    
    BTreePageGetFreeSpace();

extern bool    
    BTreePageIsSafe();

/*   
   btqual.c
   ================
*/
   
extern bool    
    ItemIdSatisfiesBTreeSearchKey();

extern bool    
    ItemIdIsLessThanBTreeSearchKey();

/*   
   btree.c
   ================
*/
   
extern void    
    InitializeBTree();

extern bool    
    RelationIsValidBTreeIndex();

extern InsertIndexResult    
    BTreeAMInsert();

extern RetrieveIndexResult    
    BTreeAMGetTuple();

extern void 
    BTreeDefaultBuild();

extern char *
    btreeinsert();

extern char *    
    btreedelete();

extern void
    btreebuild();

extern char *
    btreegetnext();

/*   
   btscan.c
   ================
*/
   
extern Index    
    BTreePageSearch();

extern ItemPointer    
    BTreeScanPageGetItemPointer();

extern Index    
    BTreeSearchKeyScanPage();

extern ItemPointer    
    BTreeSearchKeyScan();

/*   
   btsrch.c
   ================
*/
   
extern ItemMark     
    BTreeSearchKeyFindAdjacent();

extern ItemMark     
    BTreeSearchKeyFindFirst();

extern ItemMark    
    BTreeSearchKeySearch();

/*   
   btstk.c
   ================
*/
   
extern ItemPointerStack    
    GenerateItemPointerStack();

extern bool    
    ItemPointerStackIsEmpty();

extern void    
    ItemPointerStackPush();

extern ItemPointer    
    ItemPointerStackPop();

/*   
   btstrat.c
   ================
*/
   
extern StrategyNumber    
    RelationGetBTreeStrategy();

extern bool    
    RelationInvokeBTreeStrategy();

#endif /* NOTDEF */
#endif /* !BTreeExternsDefined */
