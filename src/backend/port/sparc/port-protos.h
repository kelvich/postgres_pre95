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

#include <dlfcn.h>
#include "fmgr.h"			/* for func_ptr */
#include "utils/dynamic_loader.h"

/* dynloader.c */

#define	pg_dlsym	dlsym
#define	pg_dlclose	dlclose

/* port.c */

void sparc_bug_set_outerjoincost ARGS((char *p, long val));

#endif /* PortProtos_H */
