/* ------------------------------------------
 *   FILE
 *	lispread.h
 * 
 *   DESCRIPTION
 *	prototypes for functions in lib/C/lispread.c
 *
 *   IDENTIFICATION
 *	$Header$
 * -------------------------------------------
 */

#ifndef LISPREAD_H
#define LISPREAD_H
NodeTag lispTokenType ARGS((char *token , int length ));
LispValue lispRead ARGS((bool read_car_only ));
char *lsptok ARGS((char *string , int *length ));
LispValue lispReadStringWithParams ARGS((char *string , ParamListInfo *thisParamListP ));
LispValue lispReadString ARGS((char *string ));
#endif LISPREAD_H
