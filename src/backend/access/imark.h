
; /*
; * 
; * POSTGRES Data Base Management System
; * 
; * Copyright (c) 1988 Regents of the University of California
; * 
; * Permission to use, copy, modify, and distribute this software and its
; * documentation for educational, research, and non-profit purposes and
; * without fee is hereby granted, provided that the above copyright
; * notice appear in all copies and that both that copyright notice and
; * this permission notice appear in supporting documentation, and that
; * the name of the University of California not be used in advertising
; * or publicity pertaining to distribution of the software without
; * specific, written prior permission.  Permission to incorporate this
; * software into commercial products can be obtained from the Campus
; * Software Office, 295 Evans Hall, University of California, Berkeley,
; * Ca., 94720 provided only that the the requestor give the University
; * of California a free licence to any derived software for educational
; * and research purposes.  The University of California makes no
; * representations about the suitability of this software for any
; * purpose.  It is provided "as is" without express or implied warranty.
; * 
; */



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
 * Identification:
 *	$Header$
 */

#ifndef	IMarkIncluded	/* Include this file only once. */
#define IMarkIncluded	1

#include "c.h"

#include "buf.h"
#include "itemptr.h"

typedef Pointer	ItemMark;

#define InvalidItemMark	NULL

/*
 * ItemMarkIsValid --
 *	True iff the item mark is valid.
 */
extern
bool
ItemMarkIsValid ARGS ((
	ItemMark	mark
));

/*
 * ItemMarkIsForItemPointer --
 *	True iff the item mark is associated with a disk item pointer.
 *
 * Note:
 *	Assumes the item mark is valid.
 */
extern
bool
ItemMarkIsForItemPointer ARGS ((
	ItemMark	mark
));

/*
 * ItemMarkIsForBufferPointer --
 *	True iff the item mark is associated with a buffer memory pointer.
 *
 * Note:
 *	Assumes the item mark is valid.
 */
extern
bool
ItemMarkIsForBufferPointer ARGS ((
	ItemMark	mark
));

/*
 * ItemMarkIsForCopiedPointer --
 *	True iff the item mark is associated with a non-buffer memory pointer.
 *
 * Note:
 *	Assumes the item mark is valid.
 */
extern
bool
ItemMarkIsForCopiedPointer ARGS ((
	ItemMark	mark
));

/*
 * ItemMarkGetItemPointer --
 *	Returns the item pointer associated with a disk item pointer item mark.
 *
 * Note:
 *	Assumes the item mark is for a disk item pointer.
 */
extern
ItemPointer
ItemMarkGetItemPointer ARGS((
	ItemMark	mark
));

/*
 * ItemMarkGetBuffer --
 *	Returns the buffer associated with a buffer memory pointer item mark.
 *
 * Note:
 *	Assumes the item mark is for a buffer memory pointer.
 */
extern
Buffer
ItemMarkGetBuffer ARGS((
	ItemMark	mark
));

/*
 * ItemMarkGetBufferPointer --
 *	Returns the pointer associated with a buffer memory pointer item mark.
 *
 * Note:
 *	Assumes the item mark is for a buffer memory pointer.
 */
extern
Pointer
ItemMarkGetPointer ARGS((
	ItemMark	mark
));

/*
 * ItemMarkGetCopiedPointer --
 *	Returns the pointer associated with a non-buffer pointer item mark.
 *
 * Note:
 *	Assumes the item mark is for a non-buffer memory pointer.
 */
extern
Pointer
ItemMarkGetCopiedPointer ARGS((
	ItemMark	mark
));

/*
 * ItemMarkFree --
 *	Frees a item mark.
 */
extern
void
ItemMarkFree ARGS((
	ItemMark	mark
));

/*
 * ItemPointerGetItemMark --
 *	Returns the item mark associated with a disk item pointer.
 *
 * Note:
 *	Assumes the disk item pointer is valid.
 */
extern
ItemMark
ItemPointerGetItemMark ARGS((
	ItemPointer	pointer
));

/*
 * BufferGetItemMark --
 *	Returns the item mark associated with a buffer memory pointer.
 *
 * Note:
 *	Assumes the buffer is valid.
 *	Assumes the pointer is valid.
 */
extern
ItemMark
BufferGetItemMark ARGS((
	Buffer	buffer,
	Pointer	pointer
));

/*
 * PointerGetItemMark --
 *	Returns the item mark associated with a non-buffer memory pointer.
 *
 * Note:
 *	Assumes the pointer is valid.
 */
extern
ItemMark
PointerGetItemMark ARGS((
	Pointer	pointer
));

#endif	/* !defined(IMarkIncluded) */
