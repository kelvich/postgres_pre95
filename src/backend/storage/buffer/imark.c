
/*
 * 
 * POSTGRES Data Base Management System
 * 
 * Copyright (c) 1988 Regents of the University of California
 * 
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for educational, research, and non-profit purposes and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of the University of California not be used in advertising
 * or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Permission to incorporate this
 * software into commercial products can be obtained from the Campus
 * Software Office, 295 Evans Hall, University of California, Berkeley,
 * Ca., 94720 provided only that the the requestor give the University
 * of California a free licence to any derived software for educational
 * and research purposes.  The University of California makes no
 * representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied warranty.
 * 
 */



/*
 * imark.c --
 *	POSTGRES buffer manager item marking code.
 *
 * Note:
 *	This is not implemented correctly yet.
 *	The interface should be stable, so the routines should be
 *	called as necessary.
 */

#include "c.h"

#include "buf.h"
#include "itemptr.h"

#include "imark.h"

RcsId("$Header$");

bool
ItemMarkIsValid(mark)
	ItemMark	mark;
{
	return ((bool)PointerIsValid(mark));
}

bool
ItemMarkIsForItemPointer(mark)
	ItemMark	mark;
{
	Assert(ItemMarkIsValid(mark));

	return (true);	/* XXX */
}

bool
ItemMarkIsForBufferPointer(mark)
	ItemMark	mark;
{
	Assert(ItemMarkIsValid(mark));

	return (true);	/* XXX */
}

bool
ItemMarkIsForCopiedPointer(mark)
	ItemMark	mark;
{
	Assert(ItemMarkIsValid(mark));

	return (true);	/* XXX */
}

ItemPointer
ItemMarkGetItemPointer(mark)
	ItemMark	mark;
{
	Assert(ItemMarkIsValid(mark));

	return ((ItemPointer)mark);	/* XXX */
}

Buffer
ItemMarkGetBuffer(mark)
	ItemMark	mark;
{
	Assert(ItemMarkIsValid(mark));

	return (1);	/* XXX */
}

Pointer
ItemMarkGetBufferPointer(mark)
	ItemMark	mark;
{
	Assert(ItemMarkIsValid(mark));

	return ((Pointer)mark);	/* XXX */
}

Pointer
ItemMarkGetCopiedPointer(mark)
	ItemMark	mark;
{
	Assert(ItemMarkIsValid(mark));

	return ((Pointer)mark);	/* XXX */
}

void
ItemMarkFree(mark)
	ItemMark	mark;
{
	Assert(ItemMarkIsValid(mark));

	return;	/* XXX */
}

ItemMark
ItemPointerGetItemMark(pointer)
	ItemPointer	pointer;
{
	Assert(ItemPointerIsValid(pointer));

	return ((ItemMark)pointer);	/* XXX */
}

ItemMark
BufferGetItemMark(buffer, pointer)
	Buffer	buffer;
	Pointer	pointer;
{
	Assert(BufferIsValid(buffer));
	Assert(PointerIsValid(pointer));

	return ((ItemMark)pointer);	/* XXX */
}

ItemMark
PointerGetItemMark(pointer)
	Pointer	pointer;
{
	Assert(PointerIsValid(pointer));

	return ((ItemMark)pointer);	/* XXX */
}
