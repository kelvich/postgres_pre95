/* $Header$ */

#ifndef _LOCK_H_
#define _LOCK_H_
#include "tmp/postgres.h"
#include "storage/itemptr.h"

#define INIT_TABLE_SIZE		100
#define MAX_TABLE_SIZE 		1000

typedef int LOCK_TYPE;
typedef int LOCKT;
typedef int TableId;

/* MAX_LOCKTYPES cannot be larger than the bits in MASK */
#define MAX_LOCKTYPES 5

/*
 * MAX_TABLES corresponds to the number of spin locks allocated in
 * CreateSpinLocks() or the number of shared memory locations allocated
 * for lock table spin locks in the case of machines with TAS instructions.
 */
#define MAX_TABLES 2

#define INVALID_TABLEID 0

/*typedef struct LOCK LOCK; */


typedef struct ltag {
  oid			relId;
  ItemPointerData	tupleId;
} LOCKTAG;

#define TAGSIZE (sizeof(oid)+sizeof(ItemPointerData))

#define LOCK_PRINT(where,tag)

/*
#define LOCK_PRINT(where,tag)\
  printf("%s: rel (%d) tid (%d,%d)\n",where,\
	 tag->relId,\
	 tag->tupleId.blockData,tag->tupleId.positionData);
*/
#endif /* ndef _LOCK_H_ */
