/*
 * oset.c --
 *	Fixed format ordered set definitions.
 *
 * Note:
 *	XXX This is a preliminary implementation which lacks fail-fast
 *	XXX validity checking of arguments.
 */

#include "c.h"

RcsId("$Header$");

#include "oset.h"

/*
 * OrderedElemGetBase --
 *	Returns base of enclosing structure.
 */
#define OrderedElemGetBase(elem) ((Pointer)((char*)(elem) - (elem)->set->offset))

void
OrderedSetInit(set, offset)
	OrderedSet	set;
	Offset		offset;
{
	set->head = (OrderedElem)&set->dummy;
	set->dummy = NULL;
	set->tail = (OrderedElem)&set->head;
	set->offset = offset;
}

void
OrderedElemInit(elem, set)
	OrderedElem	elem;
	OrderedSet	set;
{
	elem->set = set;
	/* mark as unattached */
	elem->next = NULL;
	elem->prev = NULL;
}

bool
OrderedSetContains(set, elem)
	OrderedSet	set;
	OrderedElem	elem;
{
	return ((bool)(elem->set == set && (elem->next || elem->prev)));
}

Pointer
OrderedSetGetHead(set)
	register OrderedSet	set;
{
	register OrderedElem	elem;

	elem = set->head;
	if (elem->next) {
		return (OrderedElemGetBase(elem));
	}
	return (NULL);
}

Pointer
OrderedSetGetTail(set)
	register OrderedSet	set;
{
	register OrderedElem	elem;

	elem = set->tail;
	if (elem->prev) {
		return (OrderedElemGetBase(elem));
	}
	return (NULL);
}

Pointer
OrderedElemGetPredecessor(elem)
	register OrderedElem	elem;
{
	elem = elem->prev;
	if (elem->prev) {
		return (OrderedElemGetBase(elem));
	}
	return (NULL);
}

Pointer
OrderedElemGetSuccessor(elem)
	register OrderedElem	elem;
{
	elem = elem->next;
	if (elem->next) {
		return (OrderedElemGetBase(elem));
	}
	return (NULL);
}

void
OrderedElemPop(elem)
	register OrderedElem	elem;
{
	elem->next->prev = elem->prev;
	elem->prev->next = elem->next;
	/* assignments used only for error detection */
	elem->next = NULL;
	elem->prev = NULL;
}

void
OrderedElemPushInto(elem, set)
	OrderedElem	elem;
	OrderedSet	set;
{
	OrderedElemInit(elem, set);
	OrderedElemPush(elem);
}

void
OrderedElemPush(elem)
	OrderedElem	elem;
{
	OrderedElemPushHead(elem);
}

void
OrderedElemPushHead(elem)
	register OrderedElem	elem;
{
	elem->next = elem->set->head;
	elem->prev = (OrderedElem)&elem->set->head;
	elem->next->prev = elem;
	elem->prev->next = elem;
}

void
OrderedElemPushTail(elem)
	register OrderedElem	elem;
{
	elem->next = (OrderedElem)&elem->set->dummy;
	elem->prev = elem->set->tail;
	elem->next->prev = elem;
	elem->prev->next = elem;
}

void
OrderedElemPushAfter(elem, oldElem)
	register OrderedElem	elem;
	register OrderedElem	oldElem;
{
	elem->next = oldElem->next;
	elem->prev = oldElem;
	oldElem->next = elem;
	elem->next->prev = elem;
}

void
OrderedElemPushBefore(elem, oldElem)
	register OrderedElem	elem;
	register OrderedElem	oldElem;
{
	elem->next = oldElem;
	elem->prev = oldElem->prev;
	oldElem->prev = elem;
	elem->prev->next = elem;
}

Pointer
OrderedSetPop(set)
	OrderedSet	set;
{
	return (OrderedSetPopHead(set));
}

Pointer
OrderedSetPopHead(set)
	register OrderedSet	set;
{
	register OrderedElem elem = set->head;

	if (elem->next) {
		OrderedElemPop(elem);
		return (OrderedElemGetBase(elem));
	}
	return (NULL);
}

Pointer
OrderedSetPopTail(set)
	register OrderedSet	set;
{
	register OrderedElem elem = set->tail;

	if (elem->prev) {
		OrderedElemPop(elem);
		return (OrderedElemGetBase(elem));
	}
	return (NULL);
}
