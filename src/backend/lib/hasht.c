/*
 * hasht.c --
 *	General-purpose hash table library code.
 *
 * Note:
 *	XXX style
 */

/*
 * XXXX 7/5/88
 * easy optimization: when searching, remeber the first invalid that you
 * pass through.  If you find what you want, switch it with the invalid.
 */

#include "c.h"

RcsId("$Header$");

#include "enbl.h"
#include "log.h"
#include "mcxt.h"

#include "hasht.h"

/* #define DEBUG_HASH		/* debug hash table inserts and deletes */

#ifdef DEBUG_HASH
# define DO_DB(A) A
#else
# define DO_DB(A) /* A */
#endif
#ifndef private
# define private /**/
#endif

#define IN()	DO_DB(ElogDebugIndentLevel++)
#define OUT()	DO_DB(ElogDebugIndentLevel--)

/*
 * The hash table is about as simple as they can get. There is
 * no chaining... just a simple hash, with a rehash of there was
 * a colision.
 *
 */

/*
 * Global State
 */
static Count		HashTableEnableCount = 0;
#define HashTableEnabled	(HashTableEnableCount >= 1)
static GlobalMemory	HashTableMemory = NULL;
static char		HashTableMemoryName[] = "HashTable";

/*
 * private functions....
 */

private void
HashTableResize ARGS((
	HashTable	hashTable
));

private Size
DefaultHashTableGrow ARGS((
	Size	size
));
	
/* 
 * when a hash slot has not yet been used, it is "EMPTY".  When
 * it has been used, but was deleted, it is "INVALID".  The reason
 * for "INVALID" slots is that you changed something to empty, then
 * searches for a particular value would abort too soon if there used
 * to be colisions along the path.
 */

#define EMPTY ((Pointer)0)
#define INVALID ((Pointer)-1)

private Size
DefaultHashTableGrow (size)
	Size	size;
{
	return size * HashTableDefaultGrowthFactor / HashTableMaxFull;
}

/*
 * EXTERNAL
 */

void
EnableHashTable(on)
	bool	on;
{
	static bool	processing = false;

	AssertState(!processing);
	AssertArg(BoolIsValid(on));

	if (BypassEnable(&HashTableEnableCount, on)) {
		return;
	}

	processing = true;

	if (on) {	/* initialize */
		EnableMemoryContext(true);

		HashTableMemory = CreateGlobalMemory(HashTableMemoryName);

	} else {	/* cleanup */
		AssertState(PointerIsValid(HashTableMemory));

		/*
		 * Ideally, the active hash tables will be in a set and
		 * any unfreed ones can be destroyed and logged as not
		 * freed normally.
		 */
		GlobalMemoryDestroy(HashTableMemory);
		HashTableMemory = NULL;

		EnableMemoryContext(false);
	}

	processing = false;
}

HashTable
CreateHashTable (hashKey, hashObject, hashSame, hashGrow, initialSize)
	Index	(*hashKey)();
	Index	(*hashObject)();
	int	(*hashSame)();
	Size	(*hashGrow)();
	Size	initialSize;
{
	HashTable	hashTable;
	Index		i;
	
	hashTable = (HashTable)MemoryContextAlloc(
		(MemoryContext)HashTableMemory, sizeof *hashTable);
	hashTable->numberObjectsInTable = 0;
	hashTable->numberEntriesInTable = 0;
	hashTable->hashTableSize = initialSize;
	hashTable->hashObject = hashObject;
	hashTable->hashKey = hashKey;
	hashTable->hashSame = hashSame;
	hashTable->disable = 0;
	hashTable->saved = NULL;
	hashTable->resizeOnNextInsert = false;

	if (hashGrow != NULL) 
		hashTable->hashGrow = hashGrow;
	else 
		hashTable->hashGrow = DefaultHashTableGrow;

	hashTable->hashTable = (Pointer *)MemoryContextAlloc(
		(MemoryContext)HashTableMemory, sizeof (Pointer) * initialSize);
	for (i = 0; i < initialSize; i++) 
		hashTable->hashTable[i] = EMPTY;

	return hashTable;
}

Pointer
HashTableInsert(hashTable, object)
	HashTable	hashTable;
	Pointer		object;
{
	uint16	collisions = 0;
	Index	i;

	Assert(hashTable->disable == 0);
	
	IN();
	DO_DB(elog(DEBUG,"HashTableInsert 0x%x 0x%x",hashTable,object));

	if (hashTable->resizeOnNextInsert) HashTableResize(hashTable);

	do {
		i = (*hashTable->hashObject)(collisions++,
			hashTable->hashTableSize,object);
	} while (hashTable->hashTable[i] != EMPTY 
		&& hashTable->hashTable[i] != INVALID);

	hashTable->numberObjectsInTable++;
	if (hashTable->hashTable[i] == EMPTY) hashTable->numberEntriesInTable++;

	hashTable->hashTable[i] = object;

	if (collisions > HashTableMaxCollisions)  
		elog(NOTICE, "HashTableInsert: bad hash function: %d collisions", collisions);

	if ( /* (collisions > HashTableMaxCollisions) || */
	    	(1<<20 * hashTable->numberEntriesInTable
	     		/ hashTable->hashTableSize)
	      > (1<<20 * HashTableMaxFull
			/ 100))
		HashTableResize(hashTable);

	OUT();
	return object;
}

/*
 * Hash Table Resize--
 *   grow the table.  This is slow, 'cause it has to re-hash 
 *   every friggin entry.
 */

private void
HashTableResize (hashTable)
	HashTable	hashTable;
{
	Size	oldSize = hashTable->hashTableSize;
	Pointer	*oldTable = hashTable->hashTable;
	Index	i;

	IN();
	hashTable->resizeOnNextInsert = false;

	hashTable->hashTableSize = 
		(*hashTable->hashGrow)(hashTable->numberObjectsInTable);

	DO_DB(elog(NOIND,"HashTableResize: %x %d %d %d %d", hashTable,
		oldSize, hashTable->hashTableSize, 
		hashTable->numberEntriesInTable,
		hashTable->numberObjectsInTable));

	hashTable->numberEntriesInTable = hashTable->numberObjectsInTable;

	Assert( hashTable->numberObjectsInTable * 1024 / hashTable->hashTableSize < (1024*HashTableMaxFull/100)); 

	hashTable->hashTable = (Pointer *)MemoryContextAlloc(
		(MemoryContext)HashTableMemory,
		sizeof (Pointer) * hashTable->hashTableSize);
	for (i = 0; i < hashTable->hashTableSize; i++) 
		hashTable->hashTable[i] = EMPTY;

	for(i = 0; i < oldSize; i++) {
		if (oldTable[i] != EMPTY && oldTable[i] != INVALID) {
			HashTableInsert(hashTable,oldTable[i]);
		}
	}

	MemoryContextFree((MemoryContext)HashTableMemory, (Pointer)oldTable);
	OUT();
}

/* VARARGS2 */
Pointer
KeyHashTableLookup (hashTable, k1, k2, k3, k4, k5, k6)
	HashTable	hashTable;
	int		k1, k2, k3, k4, k5, k6;
{
	uint16	collisions = 0;
	Size	i;

	do {
		i = (*hashTable->hashKey)(collisions++,
			hashTable->hashTableSize,k1,k2,k3,k4,k5,k6);
	} while (hashTable->hashTable[i] == INVALID 
		|| (hashTable->hashTable[i] != EMPTY
		   && (*hashTable->hashSame)(hashTable->hashTable[i],
				k1,k2,k3,k4,k5,k6)));
/* 
		printf("OUT: hashTable->hashTable[i] = %x\n",hashTable->hashTable[i]);
		if (hashTable->hashTable[i] != INVALID && hashTable->hashTable[i] != EMPTY) {
			printf("OUT: comparing.... %d\n",(*hashTable->hashSame)(hashTable->hashTable[i],k1,k2,k3,k4,k5,k6));
		}
*/

	if (hashTable->hashTable[i] == EMPTY) 
		return NULL;
	else	
		return hashTable->hashTable[i];
}

void
HashTableDelete(hashTable, object) 
	HashTable	hashTable;
	Pointer		object;
{
	Index	i;
	int32	collisions = 0;

	IN();

	DO_DB(elog(DEBUG,"HashTableDelete 0x%x 0x%x",hashTable,object));

	do {
		i = (*hashTable->hashObject)(collisions++,hashTable->hashTableSize,object);
	} while (hashTable->hashTable[i] != object 
		&& hashTable->hashTable[i] != EMPTY);

	Assert(hashTable->hashTable[i] != INVALID);
	if (hashTable->hashTable[i] == EMPTY) 
		elog(WARN,"HashTableDelete: 0x%x 0x%x not found", 
			hashTable, object);

	hashTable->hashTable[i] = INVALID;
	hashTable->numberObjectsInTable--;

	OUT();
}

void
HashTableDestroy (hashTable)
	HashTable	hashTable;
{
	MemoryContextFree((MemoryContext)HashTableMemory,
		(Pointer)hashTable->hashTable);
	MemoryContextFree((MemoryContext)HashTableMemory, (Pointer)hashTable);
}

/* VARARGS2 */
void
HashTableWalk (hashTable, function, a1, a2, a3, a4, a5, a6)
	HashTable	hashTable;
	void		(*function)();
	int		a1, a2, a3, a4, a5, a6;
{
	Pointer		data;
	int		i;
	int		oldObjects;

	hashTable->disable++;

	oldObjects = hashTable->numberObjectsInTable;

	for(i = hashTable->hashTableSize-1; i >= 0; i--) {
		data = hashTable->hashTable[i];
		if (data != EMPTY && data != INVALID) {
			(*function)(data,a1,a2,a3,a4,a5,a6);
		}
	}

	if (oldObjects - hashTable->numberObjectsInTable > 
			hashTable->hashTableSize) {
		hashTable->resizeOnNextInsert = true;
	}

	hashTable->disable--;
}


void
HashTableCover(pusher, victim)
	HashTable	pusher;
	HashTable	victim;
{
	AssertArg(!PointerIsValid(victim->saved));
	AssertArg(!PointerIsValid(pusher->saved));

	pusher->disable++;
	victim->disable++;
	
	pusher->saved = (HashTable)MemoryContextAlloc(
		(MemoryContext)HashTableMemory, sizeof *pusher);
	bcopy(victim,pusher->saved,sizeof(*pusher));

	bcopy(pusher,victim,sizeof(*pusher));

	pusher->saved = (HashTable)MemoryContextAlloc(
		(MemoryContext)HashTableMemory, sizeof *pusher);
	bcopy(pusher,pusher->saved,sizeof(*pusher));
}

void
HashTableUncover(pusher,victim)
	HashTable	pusher;
	HashTable	victim;
{
	AssertArg(PointerIsValid(victim->saved));
	AssertArg(PointerIsValid(pusher->saved));

	pusher->numberObjectsInTable = 
		pusher->numberObjectsInTable
		+ victim->numberObjectsInTable
		- pusher->saved->numberObjectsInTable;

	bcopy(victim->saved,victim,sizeof(*pusher));
	MemoryContextFree((MemoryContext)HashTableMemory,
		(Pointer)victim->saved);

	MemoryContextFree((MemoryContext)HashTableMemory,
		(Pointer)pusher->saved);

	pusher->disable--;
	victim->disable--;
}
