/* ----------------------------------------------------------------
 *   FILE
 *	slaves.h
 *	
 *   DESCRIPTION
 *	macros and definitions for parallel backends
 *
 *   NOTES
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#ifndef SlavesIncluded
#define SlavesIncluded

extern int MyPid;
#define IsMaster	(MyPid == 0)

#endif  TcopIncluded
