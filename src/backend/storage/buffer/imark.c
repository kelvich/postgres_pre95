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
