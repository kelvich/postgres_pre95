/* ----------------------------------------------------------------
 *   FILE
 *	paralleldebug.h
 *	
 *   DESCRIPTION
 *	declarations for parallel debugging purposes
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#include <usclkc.h>
struct paralleldebuginfo {
    char name[16];
    int count;
    usclk_t time;
    usclk_t start_time;
  };
#define PDI_SHAREDLOCK		0
#define PDI_EXCLUSIVELOCK	1
#define PDI_FILEREAD		2
#define PDI_FILESEEK		3

extern struct paralleldebuginfo ParallelDebugInfo[];
#define BeginParallelDebugInfo(i)     ParallelDebugInfo[i].count++; \
				      ParallelDebugInfo[i].start_time=getusclk()
#define EndParallelDebugInfo(i)		ParallelDebugInfo[i].time+=getusclk()- \
					ParallelDebugInfo[i].start_time
