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
