/*
 * taslock.h --
 *
 * Note:
 *	Included by pladt.h.
 */

#ifndef	TasLockIncluded		/* Include this file only once */
#define	TasLockIncluded	1

/*
 * Identification:
 */
#define TASLOCK_H	"$Header$"

#include "port_taslock.h"	/* from port* directory */

#define NUMPROCS	64

typedef unsigned char ubyte;

typedef struct {
    slock_t MasterLock;	/* lock is in use for read or write 	*/
    slock_t MinorLock;	/* spin-lock (monitor access)		*/
    int  NHeld;		/* number of (read) locks held		*/
    int  NWait;		/* number of processes waiting for lock */
} TasLock;

typedef struct {
    TasLock	Tas[NUMTASLOCKS];	/* normal Tas locks		  */
    TasLock	TasInit;	/* spin lock for array allocation */
    int		NumProcs;	/* this + Array_* gov't by TasInit*/
    int		Array_Pid[NUMPROCS];
    int		Array_Lck[NUMPROCS];
    char	Monitor[NUMPROCS];
} TASStructure;

#endif	/* !defined(TasLockIncluded) */
