#ifndef exceptions_h
#define exceptions_h "$Header$"

#include "utils/exc.h"
#include "utils/excid.h"

/* All raises in parser are in this form */
#define p_raise(X, Y)	raise4((X), 0, NULL, (Y))

#endif exceptions_h
