/*
 * hasht.h--
 *	General-purpose hash table library definitions.
 *
 * Note:
 *	XXX Exception handling has not been integrated with this package, yet.
 *	XXX Style
 *
 * calls:
 *
 *   CreateHashTable(
 *		hash_on_key_values, 
 *		hash_on_object, 
 *		compare_object_with_key_values, 
 *		hash_table_grow_amount, 
 *		hash_table_size)
 *
 *   HashTableInsert(hashTable, object)
 *   HashTableDelete(hashTable, object)
 *   HashTableDestroy(hashTable)
 *   KeyHashTableLookup(hashTable, key[1-6])
 *   HashTableWalk(hashTable, function, extra_args[0-6])
 *   HashTableCover(coveringHashTable, coveredHashTable)
 *   HashTableUncover(coveringHashTable, coveredHashTable)
 *
 * callback routines you must supply:
 *
 *   hash_on_object(collisions, hash_table_size, object)
 *		-- return an index into the hash array based on
 *		   object & collisions.
 *   hash_on_key_values(collisions, hash_table_size, key[1-6])
 *		-- return an index into the hash array based on
 *		   key[1-6] & collisions.  the key (up to six arguments)
 *		   must consist of the part of the object that is 
 *		   used in the hash function.  The hash function must
 *		   be identical.  (easiest implementation is to have
 *		   hash_on_object call hash_on_key_values)
 *   compare_object_with_key_values(object, key[1-6])
 *		-- this is used to check to see if an object (already
 *		   in the table matches 
 *   hash_table_grow_amount(number of object in table)
 *		-- this routine is used to find out the new size of the
 *		   hash table when it is decided that it should be made
 *		   larger.  You do not have to supply this routine (pass
 *		   a NULL instead if you don't want to bother with it)
 *
 * Identification:
 *	$Header$
 *
 */

#ifndef	HashTIncluded		/* Include this file only once */
#define HashTIncluded	1

/*
 * EnableHashTable --
 *	Enables/disables the hash table module.
 *
 * Note:
 *	This must be called after the memory context module is initialized.
 *	This must be called before any hash tables are created.
 *
 * Exceptions:
 *	BadArg if on is invalid.
 *	BadState if on is true when enabled.
 *	BadState if on is false when disabled.
 */
extern
void
EnableHashTable ARGS((
	bool	on
));

/*					C O N S T A N T S		*/

/* 
 * these are all in percents.... 
 *
 * XXXX 7/8/88   These number are *NOT* tuned. Also, they probably
 * should not be constants, but rather they should be parameters to
 * CreateHashTable()
 */

#define HashTableMaxFull 30  
	/* table can get 30% full */

#define HashTableDefaultGrowthFactor 200 
	/* when making it bigger, make room for twice the current
	 * number of entries */

#define HashTableMaxCollisions 15 
	/* if there are 15 or more collisions before inserting 
	 * an object, resize the table */

/*				D A T A  S T R U C T U R E S 		*/

/*
 * The Hash Table structure.  Note that a "HashTable" is a *pointer*...
 *
 * this package allows for multiple hash tables active at the same time.
 * when one is created, one of these structures is allocated and filled
 * in.  Note that the actual table is just an array of pointers that is
 * is pointed to by one of the fields in the structure....
 */

typedef struct HashTable {
	Pointer			*hashTable;
	Size			numberObjectsInTable;
	Size			numberEntriesInTable;
	Size			hashTableSize;
	Index			(*hashObject)();
	Index			(*hashKey)();
	Size			(*hashGrow)();
	int			(*hashSame)();
	int			disable;	/* should maybe be a flag */
	bool			resizeOnNextInsert;
	struct HashTable	*saved;
} *HashTable;


/* 				F U N C T I O N S 		*/


/*
 * Create Hash Table--
 *   create a new hash table...  see comments at top of file for 
 *   more info on the arguements
 */

HashTable
CreateHashTable ARGS((
	Index	(*hashKey)();		/* hash function applied to a key */
	Index	(*hashObject)();	/* """" object */
	int	(*hashSame)();		/* compare key and object */
	Size	(*hashGrow)();		/* return new size of table */
	Size	initialSize		/* use 3 times expected # of objects */
));

/*
 * Hash Table Insert --
 *   insert an object into the hash table.  the object is a pointer.
 *   Warning: duplicates will be faithfully inserted.  let the caller
 *   beware.
 */

Pointer
HashTableInsert ARGS((
	HashTable	hashTable;
	Pointer		object
));

/*
 * Key Hash Table Lookup--
 *   look something up in the hash table...  A pointer will be 
 *   returned if something is found, NULL otherwise.  It will 
 *   accept up to 24 bytes worth of key data as arguements after
 *   the hashTable identifier.
 */

Pointer
KeyHashTableLookup (/*
	HashTable	hashTable;
	... 		key
*/);

/*
 * Hash Table Walk --
 * 
 *   call function for each entry in the hash table.
 *   make the call like: function(datum, a1,a2,a3,a4,a5,a6)
 *   [you can have up to six extra args]
 */

void
HashTableWalk (/*
	HashTable	hashTable;
	void		(*function)();
	...		extra args
*/);

/*
 * Hash Table Delete--
 *   remove an object from the hash table.
 */

void
HashTableDelete ARGS((
	HashTable	hashTable;
	Pointer		object
));

/*
 * Hash Table Destroy--
 *   nuke the table, free the memory.
 */
 
void
HashTableDestroy ARGS((
	HashTable	hashTable
));

/*
 * Hash Table Cover
 * Hash Table Uncover
 *
 *  This function is for when you want to hide a hash table for a bit..
 *  It makes vitim look like pusher until HashTableUncover is called.
 *  You cannot stack coverings and uncoverings...
 *  
 *  Note: you cannot insert into either table when this is going on.
 *
 *  Note: if the arguments to the Cover and Uncover don't match, bad
 *  things will happen.
 *
 *  XXXXX THESE ROUTINES HAVE NEVER BEEN USED.  They have bugs.  I don't
 *  know what they are 'cause I've never called them.
 *
 */

void
HashTableCover ARGS((
	HashTable       pusher;
	HashTable       victim;
));

void
HashTableUncover ARGS((
	HashTable       pusher;
	HashTable       victim;
));

#endif	/* !defined(HashTIncluded) */
