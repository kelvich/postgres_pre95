/*
 * $Header$
 */

#include "utils/dynamic_loader.h"


#define    pg_dlsym(h, f)	((func_ptr)dl_sym(h, f))

#define    pg_dlclose(h)	dl_close(h)

