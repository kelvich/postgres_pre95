/* 
 * atoms.h
 * - string,atom lookup thingy, reduces strcmp traffic greatly
 * in the bowels of the system.  Look for actual defs in lib/C/atoms.c
 *
 * $Header$
 */

/*
#define lengthof(byte_array)	(sizeof(byte_array) / sizeof((byte_array)[0]))
#define endof(byte_array)	(&byte_array[lengthof(byte_array)])
*/
typedef struct ScanKeyword {
	char	*name;
	int	value;
} ScanKeyword;

extern ScanKeyword *ScanKeywordLookup ARGS((char *txt));
extern String AtomValueGetString ARGS((int atomval));
extern ScanKeyword	ScanKeywords[];

