/* ----------------------------------------------------------------
 *   FILE
 *	xact.h
 *	
 *   DESCRIPTION
 *	postgres transaction system header
 *
 *   NOTES
 *	
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#ifndef XactIncluded       /* include this file only once */
#define XactIncluded 1

#include <signal.h>

#include "access/xcxt.h"
#include "access/xlog.h"
#include "storage/ipci.h"
#include "tmp/miscadmin.h"
#include "tmp/portal.h"
#include "utils/log.h"
#include "utils/mcxt.h"

/* ----------------
 *	transaction state structure
 * ----------------
 */
typedef struct TransactionStateData {
    TransactionIdData	transactionIdData;
    CommandId		commandId;
    Time		startTime;
    int			state;
    int			blockState;
} TransactionStateData;

/* ----------------
 *	transaction states
 * ----------------
 */
#define TRANS_DEFAULT		0
#define TRANS_START		1
#define TRANS_INPROGRESS	2
#define TRANS_COMMIT		3
#define TRANS_ABORT		4
#define TRANS_DISABLED		5

/* ----------------
 *	transaction block states
 * ----------------
 */
#define TBLOCK_DEFAULT		0
#define TBLOCK_BEGIN		1
#define TBLOCK_INPROGRESS	2
#define TBLOCK_END		3
#define TBLOCK_ABORT		4
#define TBLOCK_ENDABORT		5

typedef TransactionStateData *TransactionState;

/* ----------------
 *	extern definitions
 * ----------------
 */
extern bool IsTransactionState ARGS(());
extern void OverrideTransactionSystem ARGS((bool flag));
extern TransactionId GetCurrentTransactionId ARGS(());
extern CommandId GetCurrentCommandId ARGS(());
extern Time GetCurrentTransactionStartTime ARGS(());
extern bool TransactionIdIsCurrentTransactionId ARGS((TransactionId xid));
extern bool CommandIdIsCurrentCommandId ARGS((CommandId cid));
extern void ClearCommandIdCounterOverflowFlag ARGS(());
extern void CommandCounterIncrement ARGS(());
extern void InitializeTransactionSystem ARGS(());
extern void AtStart_Cache ARGS(());
extern void AtStart_Locks ARGS(());
extern void AtStart_Memory ARGS(());
extern void RecordTransactionCommit ARGS(());
extern void AtCommit_Cache ARGS(());
extern void AtCommit_Locks ARGS(());
extern void AtCommit_Memory ARGS(());
extern void RecordTransactionAbort ARGS(());
extern void AtAbort_Cache ARGS(());
extern void AtAbort_Locks ARGS(());
extern void AtAbort_Memory ARGS(());
extern void StartTransaction ARGS(());
extern bool CurrentXactInProgress ARGS(());
extern void CommitTransaction ARGS(());
extern void AbortTransaction ARGS(());
extern void StartTransactionCommand ARGS(());
extern void CommitTransactionCommand ARGS(());
extern void AbortCurrentTransaction ARGS(());
extern void BeginTransactionBlock ARGS(());
extern void EndTransactionBlock ARGS(());
extern void AbortTransactionBlock ARGS(());
extern void UserErrorEndWithoutBegin ARGS(());
extern void UserErrorBeginAfterBegin ARGS(());
extern void UserErrorAbortWithoutBegin ARGS(());
extern void InternalErrorIllegalStateTransition ARGS(());
extern void FsaMachine ARGS(());
extern void InitializeTransactionState ARGS(());
extern void OverrideTransactionState ARGS(());
extern bool IsBlockTransactionState ARGS(());
extern bool IsOverrideTransactionState ARGS(());
extern void StartTransactionStateBlock ARGS(());
extern void StartTransactionStateCommand ARGS(());
extern void CommitTransactionStateCommand ARGS(());
extern void CommitTransactionStateBlock ARGS(());
extern void AbortCurrentTransactionState ARGS(());
extern void AbortTransactionStateBlock ARGS(());
extern void StartTransactionBlock ARGS(());
extern void CommitTransactionBlock ARGS(());

#endif XactIncluded
