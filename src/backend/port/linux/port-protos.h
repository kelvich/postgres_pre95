/*
 *   FILE
 *	port-protos.h
 *
 *   DESCRIPTION
 *	port-specific prototypes for SunOS 4
 *
 *   NOTES
 *
 *   IDENTIFICATION
 *	$Header$
 */

#ifndef PortProtos_H			/* include this file only once */
#define PortProtos_H 1

#include "fmgr.h"			/* for func_ptr */
#include "utils/dynamic_loader.h"

/* dynloader.c */

#define pg_dlsym(handle, funcname)	((func_ptr) dld_get_func((funcname)))
#define pg_dlclose(handle)		({ dld_unlink_by_file(handle, 1); free(handle); })


/* port.c */

#endif /* PortProtos_H */
