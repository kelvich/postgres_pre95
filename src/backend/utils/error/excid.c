/*
 * excid.c --
 *	POSTGRES known exception identifier code.
 */

#include "c.h"

#include "excid.h"

RcsId("$Header$");

Exception FailedAssertion = { "Failed Assertion" };
Exception BadState = { "Bad State for Function Call" };
Exception BadArg = { "Bad Argument to Function Call" };

Exception BadAllocSize = { "Too Large Allocation Request" };

Exception ExhaustedMemory = { "Memory Allocation Failed" };
Exception Unimplemented = { "Unimplemented Functionality" };

Exception CatalogFailure = {"Catalog failure"};	/* XXX inconsistent */
Exception InternalError = {"Internal Error"};	/* XXX inconsistent */
Exception SemanticError = {"Semantic Error"};	/* XXX inconsistent */
Exception SystemError = {"System Error"};	/* XXX inconsistent */
