/*
 *  FILE
 *	port-protos.h
 *
 *  DESCRIPTION
 *	port-specific prototypes for HP-UX
 *
 *  NOTES
 *
 *  IDENTIFICATION
 *	$Header$
 */

#ifndef PortProtos_H			/* include this file only once */
#define PortProtos_H 1

#include <dl.h>				/* for shl_t */

#include "utils/dynamic_loader.h"

/* dynloader.c */

#define	del_shlibs random		/* until i can change postinit.c */

/* pg_dl{open,close,sym} prototypes are in utils/dynamic_loader.h */

/* port.c */

double rint ARGS((double x));
double cbrt ARGS((double x));
long random ARGS((void));
void srandom ARGS((int seed));
int getrusage ARGS((int who, struct rusage *ru));

#endif /* PortProtos_H */
