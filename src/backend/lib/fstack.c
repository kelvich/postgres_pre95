/*
 * fstack.c --
 *	Fixed format stack definitions.
 */

#include "c.h"

RcsId("$Header$");

#include "fstack.h"

/*
 * Internal function definitions
 */

/*
 * FixedItemIsValid --
 *	True iff item is valid.
 */
#define FixedItemIsValid(item)	PointerIsValid(item)

/*
 * FixedStackGetItemBase --
 *	Returns base of enclosing structure.
 */
#define FixedStackGetItemBase(stack, item) \
	((Pointer)((char *)(item) - (stack)->offset))

/*
 * FixedStackGetItem --
 *	Returns item of given pointer to enclosing structure.
 */
#define FixedStackGetItem(stack, pointer) \
	((FixedItem)((char *)(pointer) + (stack)->offset))

/*
 * External functions
 */

bool
FixedStackIsValid(stack)
	FixedStack	stack;
{
	return ((bool)PointerIsValid(stack));
}

void
FixedStackInit(stack, offset)
	FixedStack	stack;
	Offset		offset;
{
	AssertArg(PointerIsValid(stack));

	stack->top = NULL;
	stack->offset = offset;
}

Pointer
FixedStackPop(stack)
	FixedStack	stack;
{
	Pointer	pointer;

	AssertArg(FixedStackIsValid(stack));

	if (!PointerIsValid(stack->top)) {
		return (NULL);
	}

	pointer = FixedStackGetItemBase(stack, stack->top);
	stack->top = stack->top->next;

	return (pointer);
}

void
FixedStackPush(stack, pointer)
	FixedStack	stack;
	Pointer		pointer;
{
	FixedItem	item = FixedStackGetItem(stack, pointer);

	AssertArg(FixedStackIsValid(stack));
	AssertArg(PointerIsValid(pointer));

	item->next = stack->top;
	stack->top = item;
}

Count
FixedStackIterate(stack, function)
	FixedStack	stack;
	void		(*function) ARGS((Pointer pointer));
{
	Pointer	pointer;
	Count	count = 0;

	/* AssertArg(FixedStackIsValid(stack)); */

	for (pointer = FixedStackGetTop(stack);
			PointerIsValid(pointer);
			pointer = FixedStackGetNext(stack, pointer)) {

		if (PointerIsValid(function)) {
			(*function)(pointer);
		}
		count += 1;
	}
	return (count);
}


bool
FixedStackContains(stack, pointer)
	FixedStack	stack;
	Pointer		pointer;
{
	FixedItem	next;
	FixedItem	item;

	AssertArg(FixedStackIsValid(stack));
	AssertArg(PointerIsValid(pointer));

	item = FixedStackGetItem(stack, pointer);

	for (next = stack->top; FixedItemIsValid(next); next = next->next) {
		if (next == item) {
			return (true);
		}
	}
	return (false);
}

Pointer
FixedStackGetTop(stack)
	FixedStack	stack;
{
	AssertArg(FixedStackIsValid(stack));

	if (!PointerIsValid(stack->top)) {
		return (NULL);
	}

	return (FixedStackGetItemBase(stack, stack->top));
}

Pointer
FixedStackGetNext(stack, pointer)
	FixedStack	stack;
	Pointer		pointer;
{
	FixedItem	item;

	/* AssertArg(FixedStackIsValid(stack)); */
	/* AssertArg(PointerIsValid(pointer)); */
	AssertArg(FixedStackContains(stack, pointer));

	item = FixedStackGetItem(stack, pointer)->next;

	if (!PointerIsValid(item)) {
		return (NULL);
	}

	return(FixedStackGetItemBase(stack, item));
}
