/*
 * enbl.c --
 *	POSTGRES module enable and disable support code.
 */

#include "tmp/c.h"

RcsId("$Header$");

#include "utils/module.h"

bool
BypassEnable(enableCountInOutP, on)
	Count	*enableCountInOutP;
	bool	on;
{
	AssertArg(PointerIsValid(enableCountInOutP));
	AssertArg(BoolIsValid(on));

	if (on) {
		*enableCountInOutP += 1;
		return ((bool)(*enableCountInOutP >= 2));
	}

	AssertState(*enableCountInOutP >= 1);

	*enableCountInOutP -= 1;

	return ((bool)(*enableCountInOutP >= 1));
}
