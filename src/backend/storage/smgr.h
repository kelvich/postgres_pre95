/*
 *  smgr.h -- storage manager switch public interface declarations.
 *
 *	$Header$
 */

#ifndef _SMGR_H_

#define SM_FAIL		0
#define	SM_SUCCESS	1

#define	DEFAULT_SMGR	0

extern int	smgrinit();
extern void	smgrshutdown();
extern int	smgrcreate();
extern int	smgrunlink();
extern int	smgrextend();
extern int	smgropen();
extern int	smgrclose();
extern int	smgrread();
extern int	smgrwrite();
extern int	smgrflush();
extern int	smgrblindwrt();
extern int	smgrnblocks();
extern int	smgrcommit();
extern int	smgrabort();

#endif /* ndef _SMGR_H_ */
