/*
 *  FILE
 *	port-protos.h
 *
 *  DESCRIPTION
 *
 *  INTERFACE ROUTINES
 *
 *  NOTES
 *	Based on:
 *	$Header: hpux port-protos.h Rik Tue Jan 19 17:11:46 GMT 1993 
 *
 *  IDENTIFICATION
 *	$Header$
 */

#include "fmgr.h"
#include "utils/dynamic_loader.h"

/* dfmgr.c */
void		    del_shlibs ARGS((int code));

/* dynloader.c */

DynamicFunctionList *read_symbols ARGS((char *filename ));
func_ptr 	    dynamic_load ARGS((char **err ));
int       	    execld ARGS((char *tmp_file , char *filename ));
void      	    free_dyna ARGS(( DynamicFunctionList **dyna_list ));

typedef struct dfHandle {
    DynamicFunctionList *func_list;
    shl_t handle;			/* Handle for shared library */
    char shlib[MAXPATHLEN];		/* File of dynamic loader */
} dfHandle;

/* port.c */

double rint ARGS((double x));
double cbrt ARGS((double x));
long random ARGS((void));
void srandom ARGS((int seed));
int getrusage ARGS((int who, struct rusage *ru));
