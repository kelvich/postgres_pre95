/*
 * $Header$
 */

#include "utils/dynamic_loader.h"

#define	 pg_dlsym(h, f)		((func_ptr)dlsym(h, f))

#define	 pg_dlclose(h)		dlclose(h)
