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

/* pg_dl{open,close,sym} prototypes are in utils/dynamic_loader.h */

/* port.c */

extern int init_address_fixup ARGS((void));
extern double rint ARGS((double x));
extern double cbrt ARGS((double x));
extern long random ARGS((void));
extern void srandom ARGS((int seed));
extern int getrusage ARGS((int who, struct rusage *ru));

#endif /* PortProtos_H */
