/*
 *   FILE
 *	port-protos.h
 *
 *   DESCRIPTION
 *	prototypes for OSF/1-specific routines
 *
 *   NOTES
 *	OSF/1 provides the dlopen() interface, so most of the pg_dlopen()
 *	stuff maps directly into it.
 *
 *   IDENTIFICATION
 *	$Header$
 */

#include "utils/dynamic_loader.h"

/* dynloader.c */

#define	 pg_dlsym(h, f)		((func_ptr)dlsym(h, f))
#define	 pg_dlclose(h)		dlclose(h)

/* port.c */

extern init_address_fixup ARGS((void));
