/* ----------------------------------------------------------------
 *      FILE
 *     	tcopdebug.h
 *     
 *      DESCRIPTION
 *     	#defines governing debugging behaviour in the traffic cop
 *
 *	$Header$
 * ----------------------------------------------------------------
 */

#ifndef TcopDebugIncluded
#define TcopDebugIncluded

/* ----------------------------------------------------------------
 *	debugging defines.
 *
 *	If you want certain debugging behaviour, then #define
 *	the variable to 1, else #undef it. -cim 10/26/89
 * ----------------------------------------------------------------
 */

/* ----------------
 *	TCOP_SLAVESYNCDEBUG is a #define which causes the
 *	traffic cop to print slave backend synchronization
 *	messages.
 * ----------------
 */
#undef TCOP_SLAVESYNCDEBUG

/* ----------------
 *	TCOP_SHOWSTATS controls whether or not buffer and
 *	access method statistics are shown for each query.  -cim 2/9/89
 * ----------------
 */
#undef TCOP_SHOWSTATS

/* ----------------------------------------------------------------
 *	#defines controlled by above definitions
 * ----------------------------------------------------------------
 */

/* ----------------
 *	slave synchronization debugging defines
 * ----------------
 */
#ifdef TCOP_SLAVESYNCDEBUG
#define SLAVE_elog(l, s)		elog(l, s)
#define SLAVE1_elog(l, s, a)		elog(l, s, a)
#else
#define SLAVE_elog(l, s)	
#define SLAVE1_elog(l, s, a)	
#endif TCOP_SLAVESYNCDEBUG

#endif  TcopDebugIncluded
