/*
 *   FILE
 *	port-protos.h
 *
 *   DESCRIPTION
 *	prototypes for Ultrix-specific routines
 *
 *   NOTES
 *	Since Andrew's libdl maps into the regular SVIDish dlopen() 
 *	interface, we can just #define some of the pg_dlopen() stuff 
 *	into the dlopen() routines.
 *
 *   IDENTIFICATION
 *	$Header$
 */

#include "utils/dynamic_loader.h"

/* dynloader.c */

#define    pg_dlsym(h, f)	((func_ptr)dl_sym(h, f))
#define    pg_dlclose(h)	dl_close(h)

/* port.c */

extern init_address_fixup ARGS((void));
