
/*
 *	DYNAMIC_LOADER.H
 *
 *	$Header$
 */

#ifndef CIncluded
#include "c.h"
#endif

typedef char *	((*func_ptr)());

func_ptr	dynamic_load();

typedef struct {
    func_ptr    func;
    char        *name;
} FList;

extern FList ExtSyms[];

