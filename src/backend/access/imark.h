/*
 * imark.h --
 *	POSTGRES buffer manager item marking definitions.
 *
 * Explaination:
 *	There are three types of disk items which need to be
 *	manipulated.  The first type is disk item pointers.
 *	The second type is pointers to items on a disk block
 *	(buffer frame).  The third is pointers to copies of
 *	disk resident data (before or after transformations).
 *
 *	The first type of disk item is represented by the
 *	disk item pointer and an "unknown" buffer.
 *
 *	The second type of disk item is represented by the
 *	pointer and its associated pinned buffer.
 *
 *	The third type of disk item is represented by the
 *	pointer	and an "invalid" buffer.  This type of item
 *	is especially easy to manage because such an item
 *	remains valid even after a relation restructuring.
 *
 * XXX
 *	None of this is implemented, so I replaced the dummy functions
 *	with macros.  -cim 4/27/91
 *
 * Identification:
 *	$Header$
 */

#ifndef	IMarkIncluded	/* Include this file only once. */
#define IMarkIncluded	1

#include "tmp/c.h"
#include "storage/buf.h"
#include "storage/itemptr.h"

typedef Pointer	ItemMark;

#define InvalidItemMark	NULL

/*
 * ItemMarkIsValid --
 *	True iff the item mark is valid.
 */
#define ItemMarkIsValid(mark) PointerIsValid(mark)

/*
 * ItemMarkIsForItemPointer --
 *	True iff the item mark is associated with a disk item pointer.
 *
 * Note:
 *	Does nothing at present. XXX
 */
#define ItemMarkIsForItemPointer(mark) \
    ((bool) AssertMacro(ItemMarkIsValid(mark)))

/*
 * ItemMarkIsForBufferPointer --
 *	True iff the item mark is associated with a buffer memory pointer.
 *
 * Note:
 *	Does nothing at present. XXX
 */
#define ItemMarkIsForBufferPointer(mark) \
    ((bool) AssertMacro(ItemMarkIsValid(mark)))

/*
 * ItemMarkIsForCopiedPointer --
 *	True iff the item mark is associated with a non-buffer memory pointer.
 *
 * Note:
 *	Does nothing at present. XXX
 */
#define ItemMarkIsForCopiedPointer(mark) \
    ((bool) AssertMacro(ItemMarkIsValid(mark)))

/*
 * ItemMarkGetItemPointer --
 *	Returns the item pointer associated with a disk item pointer item mark.
 *
 * Note:
 *	Assumes the item mark is for a disk item pointer.
 *	Presently just returns it's argument.  XXX
 */
#define ItemMarkGetItemPointer(mark) \
    ((ItemPointer) \
     AssertMacro(ItemMarkIsValid(mark)) ? \
     (ItemPointer) mark : (ItemPointer) NULL)

/*
 * ItemMarkGetBuffer --
 *	Returns the buffer associated with a buffer memory pointer item mark.
 *
 * Note:
 *	Assumes the item mark is for a buffer memory pointer.
 *	Does nothing at present.  XXX
 */
#define ItemMarkGetBuffer(mark) \
    ((Buffer) \
     AssertMacro(ItemMarkIsValid(mark)) ? (Buffer) 1 : (Buffer) 0)

/*
 * ItemMarkGetBufferPointer --
 *	Returns the pointer associated with a buffer memory pointer item mark.
 *
 * Note:
 *	Assumes the item mark is for a buffer memory pointer.
 *	Does nothing at present.  XXX
 */
#define ItemMarkGetBufferPointer(mark) \
    ((ItemPointer) \
     AssertMacro(ItemMarkIsValid(mark)) ? \
     (ItemPointer) mark : (ItemPointer) NULL)

/*
 * ItemMarkGetCopiedPointer --
 *	Returns the pointer associated with a non-buffer pointer item mark.
 *
 * Note:
 *	Assumes the item mark is for a non-buffer memory pointer.
 *	Does nothing at present.  XXX
 */
#define ItemMarkGetCopiedPointer(mark) \
    ((ItemPointer) \
     AssertMacro(ItemMarkIsValid(mark)) ? \
     (ItemPointer) mark : (ItemPointer) NULL)

/*
 * ItemMarkFree --
 *	Frees a item mark.
 *	Does nothing at present.  XXX
 */
#define ItemMarkFree(mark) \
    Assert(ItemMarkIsValid(mark))

/*
 * ItemPointerGetItemMark --
 *	Returns the item mark associated with a disk item pointer.
 *
 * Note:
 *	Assumes the disk item pointer is valid.
 *	Does nothing at present.  XXX
 */
#define ItemPointerGetItemMark(pointer) \
    ((ItemMark) \
     AssertMacro(ItemPointerIsValid(pointer)) ? \
     (ItemMark) pointer : (ItemMark) NULL)

/*
 * BufferGetItemMark --
 *	Returns the item mark associated with a buffer memory pointer.
 *
 * Note:
 *	Assumes the buffer is valid.
 *	Assumes the pointer is valid.
 *	Does nothing at present.  XXX
 */
#define BufferGetItemMark(buffer, pointer) \
    ((ItemMark) \
     (AssertMacro(BufferIsValid(buffer) && \
		  PointerIsValid(pointer)) ? \
      (ItemMark) pointer : (ItemMark) NULL))

/*
 * PointerGetItemMark --
 *	Returns the item mark associated with a non-buffer memory pointer.
 *
 * Note:
 *	Assumes the pointer is valid.
 *	Does nothing at present.  XXX
 */
#define PointerGetItemMark(pointer) \
    ((ItemMark) \
     (AssertMacro(PointerIsValid(pointer)) ? \
      (ItemMark) pointer : (ItemMark) NULL))

#endif	/* !defined(IMarkIncluded) */
