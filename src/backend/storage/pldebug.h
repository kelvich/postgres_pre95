
; /*
; * 
; * POSTGRES Data Base Management System
; * 
; * Copyright (c) 1988 Regents of the University of California
; * 
; * Permission to use, copy, modify, and distribute this software and its
; * documentation for educational, research, and non-profit purposes and
; * without fee is hereby granted, provided that the above copyright
; * notice appear in all copies and that both that copyright notice and
; * this permission notice appear in supporting documentation, and that
; * the name of the University of California not be used in advertising
; * or publicity pertaining to distribution of the software without
; * specific, written prior permission.  Permission to incorporate this
; * software into commercial products can be obtained from the Campus
; * Software Office, 295 Evans Hall, University of California, Berkeley,
; * Ca., 94720 provided only that the the requestor give the University
; * of California a free licence to any derived software for educational
; * and research purposes.  The University of California makes no
; * representations about the suitability of this software for any
; * purpose.  It is provided "as is" without express or implied warranty.
; * 
; */



/*
 * pldebug.h --
 *	POSTGRES lock manager debugging definitions.
 *
 * Identification:
 *	$Header$
 */

#ifndef	PLDebugIncluded     /* Include this file only once */
#define PLDebugIncluded     1

#include <sys/file.h>
#include <stdio.h>


#define ERROR     1
#define LDEBUG    2
/* #define DEBUG      2  /* !! */
#define DEBERR	  3
#define STATISTIC 4
#define LOCK	  5

#define LOCK_LOG_FILE  "LtLockLog"  	/* the shared lock log file name*/
#define SYNCH_LOG_FILE   "LtSyncLog"	/* the trace file name	    	*/

extern	void	DebugOn();
extern	void	DebugOff();
extern	int 	DebugIsOn();
extern	void	DebugErrorOn();
extern	void	DebugErrorOff();
extern  int 	DebugErrorIsOn();
extern  void	DebugFileOn();
extern  void	DebugFileOff();
extern  int 	DebugFileIsOn();
extern  void    DebugStderrOn();
extern  void    DebugStderrOff();
extern  int     DebugStderrIsOn();
extern	void	LockLogFileInit();

#endif	/*!defined(PLDebugIncluded) */
