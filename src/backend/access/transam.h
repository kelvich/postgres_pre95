/* ----------------------------------------------------------------
 *   FILE
 *	transam.h
 *	
 *   DESCRIPTION
 *	postgres transaction access method support code header
 *
 *   NOTES
 *	
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#ifndef TransamIncluded       /* include this file only once */
#define TransamIncluded 1

#include <sys/file.h>
#include <strings.h>
#include <math.h>
#include <stdio.h>

#include "tmp/postgres.h"	/* for struct varlena, etc. */

#include "access/att.h"
#include "access/attnum.h"
#include "tmp/bit.h"
#include "storage/block.h"
#include "storage/buf.h"
#include "storage/bufmgr.h"
#include "storage/bufpage.h"
#include "catalog/catname.h"
#include "tmp/datum.h"
#include "access/heapam.h"
#include "access/htup.h"
#include "utils/log.h"
#include "utils/mcxt.h"
#include "tmp/name.h"
#include "utils/rel.h"
#include "access/relscan.h"
#include "rules/rlock.h"
#include "access/skey.h"
#include "access/tupdesc.h"
#include "tmp/xid.h"

/* ----------------
 *	transaction system version id
 *
 *	this is stored on the first page of the log, time and variable
 *	relations on the first 4 bytes.  This is so that if we improve
 *	the format of the transaction log after postgres version 2, then
 *	people won't have to rebuild their databases.
 *
 *	TRANS_SYSTEM_VERSION 100 means major version 1 minor version 0.
 *	Two databases with the same major version should be compatible,
 *	even if their minor versions differ.
 * ----------------
 */
#define TRANS_SYSTEM_VERSION	100

/* ----------------
 *	transaction id status values
 *
 *	someday we will use "11" = 3 = XID_INVALID to mean the
 *	starting of run-length encoded log data.
 * ----------------
 */
#define XID_COMMIT      2       		/* transaction commited */
#define XID_ABORT       1       		/* transaction aborted */
#define XID_INPROGRESS  0       		/* transaction in progress */
#define XID_INVALID     3       		/* other */

typedef unsigned char XidStatus; 		/* (2 bits) */

/* ----------------
 *   	BitIndexOf computes the index of the Nth xid on a given block
 * ----------------
 */
#define BitIndexOf(N)   ((N) * 2)

/* ----------------
 *	transaction page definitions
 * ----------------
 */
#define TP_DataSize		BLCKSZ
#define TP_NumXidStatusPerBlock	(TP_DataSize * 4)
#define TP_NumTimePerBlock	(TP_DataSize / 4)

/* ----------------
 *	LogRelationContents structure
 *
 *	This structure describes the storage of the data in the
 *	first 128 bytes of the log relation.  This storage is never
 *	used for transaction status because transaction id's begin
 *	their numbering at 512.
 *
 *	The first 4 bytes of this relation store the version
 *	number of the transction system.
 * ----------------
 */
typedef struct LogRelationContentsData {
   int			TransSystemVersion;
} LogRelationContentsData;

typedef LogRelationContentsData *LogRelationContents;

/* ----------------
 *	TimeRelationContents structure
 *
 *	This structure describes the storage of the data in the
 *	first 2048 bytes of the time relation.  This storage is never
 *	used for transaction commit times because transaction id's begin
 *	their numbering at 512.
 *
 *	The first 4 bytes of this relation store the version
 *	number of the transction system.
 * ----------------
 */
typedef struct TimeRelationContentsData {
   int			TransSystemVersion;
} TimeRelationContentsData;

typedef TimeRelationContentsData *TimeRelationContents;

/* ----------------
 *	VariableRelationContents structure
 *
 *	The variable relation is a special "relation" which
 *	is used to store various system "variables" persistantly.
 *	Unlike other relations in the system, this relation
 *	is updated in place whenever the variables change.
 *
 *	The first 4 bytes of this relation store the version
 *	number of the transction system.
 *
 *	Currently, the relation has only one page and the next
 *	available xid and the last committed xid are stored there.
 * ----------------
 */
typedef struct VariableRelationContentsData {
   int			TransSystemVersion;
   TransactionIdData	nextXidData;
   TransactionIdData	lastXidData;
} VariableRelationContentsData;

typedef VariableRelationContentsData *VariableRelationContents;

/* ----------------
 *	extern declarations
 * ----------------
 */
  
/* ----------------
 *	global variable extern declarations
 * ----------------
 */
extern Relation	LogRelation;
extern Relation	TimeRelation;
extern Relation	VariableRelation;

extern bool AMI_OVERRIDE;

#endif TramsamIncluded
