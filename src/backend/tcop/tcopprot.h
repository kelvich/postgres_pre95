
/* $Header$ */

/*
 * This file was created so that other c files could get the two
 * function prototypes without having to include tcop.h which single
 * handedly includes the whole f*cking tree -- mer 5 Nov. 1991
 */

#ifndef _tcopprot_included_
#define _tcopprot_included_

/* ----------------
 *	prototypes for externs
 * ----------------
 */

extern void die ARGS((void));
extern void handle_warn ARGS((void));

#endif _tcopprot_included_
