
/*
 *	DYNAMIC_LOADER.H
 *
 *	$Header$
 */

#ifndef Dynamic_loaderHIncluded
#define Dynamic_loaderHIncluded 1 /* include once only */

#include "tmp/c.h"

typedef char *	((*func_ptr)());

func_ptr	dynamic_load();

typedef struct {
    func_ptr    func;
    char        *name;
} FList;

extern FList ExtSyms[];

#endif Dynamic_loaderHIncluded
