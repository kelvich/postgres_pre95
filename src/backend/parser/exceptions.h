#define exceptions_h "$Header$"

#include "exc.h"
#include "excid.h"

/* All raises in parser are in this form */
#define p_raise(X, Y)	raise4((X), 0, NULL, (Y))

