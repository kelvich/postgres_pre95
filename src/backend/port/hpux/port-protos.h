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

DynamicFunctionList *dynamic_file_load 
	ARGS((char **err , char *filename , char **libname , shl_t *handle ));

DynamicFunctionList *read_symbols ARGS((char *filename ));
func_ptr 	    dynamic_load ARGS((char **err ));
int       	    execld ARGS((char *tmp_file , char *filename ));
void      	    free_dyna ARGS(( DynamicFunctionList **dyna_list ));

/* port.c */

double rint ARGS((double x));
double cbrt ARGS((double x));
long random ARGS((void));
void srandom ARGS((int seed));
int getrusage ARGS((int who, struct rusage *ru));
