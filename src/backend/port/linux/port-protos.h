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
 *	/usr/local/devel/postgres-4.2-devel/src/backend/port/sparc/RCS/port-protos.h,v 1.5 1993/08/04 11:02:54 aoki Exp
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
