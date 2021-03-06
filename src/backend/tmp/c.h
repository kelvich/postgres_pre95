/* ----------------------------------------------------------------
 *   FILE
 *	c.h
 *
 *   DESCRIPTION
 *	Fundamental C definitions.  This is included by 
 *	every .c file in postgres.
 *
 *   TABLE OF CONTENTS
 *
 *	When adding stuff to this file, please try and put stuff
 *	into the relevent section, or add new sections as appropriate.
 *
 *    section	description
 *    -------	------------------------------------------------
 *	1)	palloc debugging defines
 *	2)	bool, true, false, TRUE, FALSE
 *	3)	__STDC__, non-ansi C definitions:
 *		Pointer typedef, NULL
 *		cpp magic macros
 *		ARGS() prototype macro
 *		type prefixes: const, signed, volatile, inline
 *
 *	4)	standard system types
 *	5)	malloc, free, malloc_debug stuff, new, delete, ALLOCATE
 *	6)	IsValid macros for system types
 *	7)	offsetof, lengthof, endof 
 *	8)	exception handling definitions, Assert, Trap, etc macros
 *	9)	Min, Max, Abs macros
 *	10)	LintCast, RevisionId, RcsId, SccsId macros
 *	11)	<dead>
 *	12)	externs
 *	13)	system-specific hacks
 *
 *   NOTES
 *	This file reorganized as a result of eliminating wastful
 *	nested IsValid calls -cim 4/27/91
 *
 *	This file is MACHINE AND COMPILER dependent!!!  (For now.)
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#ifndef	CIncluded		/* Include this file only once */
#define CIncluded	1

#define C_H	"$Header$"

/* ----------------------------------------------------------------
 *		Section 1:  palloc debugging defines
 * ----------------------------------------------------------------
 */

/* ----------------
 *	file / line macros
 *
 *	_FLD_ 		File/Line Data
 *	_FLV_ 		File/Line Variables
 *	_FLV_DCL_ 	File/Line Variable Declarations
 *
 *	the FLD0 and FLV0 macros are for functions taking no arguments
 * ----------------
 */
#define _FLD_ 		__FILE__, __LINE__, 
#define _FLD0_ 		__FILE__, __LINE__
#define _FLV_ 		_FL_file, int _FL_line,
#define _FLV0_ 		_FL_file, int _FL_line
#define _FLV_DCL_ 	char *_FL_file; int _FL_line;
#define _FL_PRINT_	printf("f: %s l: %d ", _FL_file, _FL_line)

/* ----------------
 *	storage managers
 *
 *	These are experimental and are not supported in the code that
 *	we distribute to other sites.
 * ----------------
 */
#ifdef SEQUOIA
#define SONY_JUKEBOX
#define MAIN_MEMORY
#define JAQUITH
#endif

/* ----------------
 *	allocation debugging stuff
 * ----------------
 */
#undef PALLOC_DEBUG 

#ifdef PALLOC_DEBUG
#define palloc(size)       palloc_debug(_FLD_ size)
#define pfree(ptr)         pfree_debug(_FLD_ ptr)
#define MemoryContextAlloc(context, size) \
	MemoryContextAlloc_Debug(_FLD_ context, size)
#define MemoryContextFree(context, ptr)  \
	MemoryContextFree_Debug(_FLD_ context, ptr)
#define AllocSetReset(set) AllocSetReset_debug(_FLD_ set)
#define malloc(size)	    malloc_debug(_FLD_ size)
#define free(ptr)	    free_debug(_FLD_ ptr)
#endif /* PALLOC_DEBUG */

/* -------------------
 *	buffer pool pin count debugging stuff
 * -------------------
 */
#undef BUFMGR_DEBUG

#ifdef BUFMGR_DEBUG
#define IncrBufferRefCount(buffer)	IncrBufferRefCount_Debug(_FLD_ buffer)
#define ReleaseBuffer(buffer)		ReleaseBuffer_Debug(_FLD_ buffer)
#define ReleaseAndReadBuffer(buffer, relation, blockNum) \
		ReleaseAndReadBuffer_Debug(_FLD_ buffer, relation, blockNum)
#define ReadBuffer(reln, blockNum)	ReadBuffer_Debug(_FLD_ reln, blockNum)
#define WriteBuffer(buffer)		WriteBuffer_Debug(_FLD_ buffer)
/*
#define PinBuffer(buf)			PinBuffer_Debug(_FLD_ buf)
#define UnpinBuffer(buf)		UnpinBuffer_Debug(_FLD_ buf)
*/
#endif /* BUFMGR_DEBUG */

/*
 * Begin COMPILER DEPENDENT section
 */

/* ----------------------------------------------------------------
 *		Section 2:  bool, true, false, TRUE, FALSE
 * ----------------------------------------------------------------
 */
/*
 * bool --
 *	Boolean value, either true or false.
 *
 * used to be:
 *
 * typedef enum bool {
 *	false,		must be first, to be 0
 *	true
 * } bool;
 *
 * this may soon be moved to postgres.h -cim
 */
#define bool	char
#define false	((char) 0)
#define true	((char) 1)
typedef bool	*BoolPtr;

#ifndef TRUE
#define TRUE	1
#endif /* TRUE */
#ifndef FALSE
#define FALSE	0
#endif /* FALSE */

/* ----------------------------------------------------------------
 *		Section 3: __STDC__, non-ansi C definitions:
 *
 *		cpp magic macros
 *		ARGS() prototype macro
 *		Pointer typedef, NULL
 *		type prefixes: const, signed, volatile, inline
 * ----------------------------------------------------------------
 */

#ifdef	__STDC__ /* ANSI C */

#if defined(__GNUC__)
#define PROTOTYPES
#endif

/*
 * Pointer --
 *	Variable holding address of any memory resident object.
 */
#if defined(__GNUC__)
typedef void	*Pointer;
#else /* __GNUC__ */
/*
 *	XXX Pointer arithmetic is done with this, so it can't be void *
 *	under "true" ANSI compilers.
 */
typedef char	*Pointer;
#endif /* __GNUC__ */

#ifndef	NULL
/*
 * NULL --
 *	Null pointer.
 */
#define NULL	((void *) 0)
#endif	/* !defined(NULL) */

#define	HAVE_ANSI_CPP	/* all ANSI C compilers must have this! */
#if defined(NEED_STD_HDRS)
#undef NEED_STD_HDRS	/* all ANSI systems must have stddef/stdlib */
#endif /* NEED_STD_HDRS */

#else	/* !defined(__STDC__) */ /* NOT ANSI C */

/*
 * Pointer --
 *	Variable containing address of any memory resident object.
 */
typedef char	*Pointer;

#ifndef	NULL
/*
 * NULL --
 *	Null pointer.
 */
#define NULL	0
#endif	/* !defined(NULL) */

/*
 * const --
 *	Type modifier.  Identifies read only variables.
 *
 * Example:
 *	extern const Version	RomVersion;
 */
#define const		/* const */

/*
 * signed --
 *	Type modifier.  Identifies signed integral types.
 */
#define signed		/* signed */

/*
 * volatile --
 *	Type modifier.  Identifies variables which may change in ways not
 *	noticeable by the compiler, e.g. via asynchronous interrupts.
 *
 * Example:
 *	extern volatile unsigned int	NumberOfInterrupts;
 */
#define volatile	/* volatile */

#endif	/* !defined(__STDC__) */ /* NOT ANSI C */

/*
 * CppAsString --
 *	Convert the argument to a string, using the C preprocessor.
 * CppConcat --
 *	Concatenate two arguments together, using the C preprocessor.
 */
#if defined(HAVE_ANSI_CPP)

#define CppAsString(identifier)	#identifier
#define CppConcat(x, y)		x##y
#define CppConcat0(x, y)	x##y
#define CppConcat1(x, y)	x##y
#define CppConcat2(x, y)	x##y
#define CppConcat3(x, y)	x##y
#define CppConcat4(x, y)	x##y

#else /* !HAVE_ANSI_CPP */

#define CppAsString(identifier)	"identifier"

/*
 * CppIdentity -- On Reiser based cpp's this is used to concatenate
 *	two tokens.  That is
 *		CppIdentity(A)B	==> AB
 *	We renamed it to _private_CppIdentity because it should not
 *	be referenced outside this file.  On other cpp's it
 *	produces  A  B.
 */
#define _priv_CppIdentity(x)x
#define CppConcat(x, y)		_priv_CppIdentity(x)y
#define CppConcat0(x, y)	_priv_CppIdentity(x)y
#define CppConcat1(x, y)	_priv_CppIdentity(x)y
#define CppConcat2(x, y)	_priv_CppIdentity(x)y
#define CppConcat3(x, y)	_priv_CppIdentity(x)y
#define CppConcat4(x, y)	_priv_CppIdentity(x)y

#endif /* !HAVE_ANSI_CPP */

#ifndef __GNUC__	/* GNU cc */
# define inline
#endif

#ifdef PROTOTYPES
#define ARGS(args)	args
#else
#define ARGS(args)	(/*args*/)
#endif /* PROTOTYPES */

#if defined(NEED_STD_HDRS)
/*
 * You're doomed.  We've removed almost all of our own C library 
 * extern declarations because they conflict on the different
 * systems.  You'll have to write your own stdlib.h.
 */
#include "stdlib.h"
#else /* NEED_STD_HDRS */
#include <stddef.h>
#include <stdlib.h>
#endif /* NEED_STD_HDRS */

/* ----------------------------------------------------------------
 *		Section 4:  standard system types
 * ----------------------------------------------------------------
 */

/*
 * End COMPILER DEPENDENT section
 */


/*
 * Begin COMPILER AND HARDWARE DEPENDENT section
 */

/*
 * intN --
 *	Signed integer, AT LEAST N BITS IN SIZE,
 *	used for numerical computations.
 */
typedef signed char	int8;		/* >= 8 bits */
typedef signed short	int16;		/* >= 16 bits */
typedef signed int	int32;		/* >= 32 bits */

/*
 * AsInt8 --
 *	Coerce the argument to int8.
 *
 * Note:
 *	This macro must be used to reference all int8 variables, because
 *
 *		o Not all compilers support the signed char type.
 *		o Depending on the compiler and/or machine, char
 *		  types can be either signed or unsigned.
 */
#define AsInt8(n)	((int) (((n) & 0x80) ? (~0x7f | (n)) : (n)))

/*
 * uintN --
 *	Unsigned integer, AT LEAST N BITS IN SIZE,
 *	used for numerical computations.
 */
typedef unsigned char	uint8;		/* >= 8 bits */
typedef unsigned short	uint16;		/* >= 16 bits */
typedef unsigned int	uint32;		/* >= 32 bits */

/*
 * floatN --
 *	Floating point number, AT LEAST N BITS IN SIZE,
 *	used for numerical computations.
 *
 *	Since sizeof(floatN) may be > sizeof(char *), always pass
 *	floatN by reference.
 */
typedef float		float32data;
typedef double		float64data;
typedef float		*float32;
typedef double		*float64;

/*
 * AsUint8 --
 *	Coerce the argument to uint8.
 *
 * Note:
 *	This macro must be used to reference all int8 variables, because
 *
 *		o Not all compilers support the unsigned char type.
 *		o Depending on the compiler and/or machine, char
 *		  types can be either signed or unsigned.
 */
#define AsUint8(n)	((unsigned) ((n) & 0xff))

/*
 * boolN --
 *	Boolean value, AT LEAST N BITS IN SIZE.
 */
typedef uint8		bool8;		/* >= 8 bits */
typedef uint16		bool16;		/* >= 16 bits */
typedef uint32		bool32;		/* >= 32 bits */

/*
 * bitsN --
 *	Unit of bitwise operation, AT LEAST N BITS IN SIZE.
 */
typedef uint8		bits8;		/* >= 8 bits */
typedef uint16		bits16;		/* >= 16 bits */
typedef uint32		bits32;		/* >= 32 bits */

/*
 * wordN --
 *	Unit of storage, AT LEAST N BITS IN SIZE,
 *	used to fetch/store data.
 */
typedef uint8		word8;		/* >= 8 bits */
typedef uint16		word16;		/* >= 16 bits */
typedef uint32		word32;		/* >= 32 bits */

/*
 * IoChar --
 *	Either a character or EOF, as returned by getchar.
 */
typedef int		IoChar;

/*
 * Address --
 *	Address of any memory resident object.
 *
 * Note:
 *	This differs from Pointer type in that an object of type
 *	Pointer contains an address, whereas an object of type
 *	Address is an address (e.g. a constant).
 *
 * Example:
 *	extern Address	EndOfMemory;
 *	extern Pointer	TopOfStack;
 */
typedef char		Address[];

/*
 * Size --
 *	Size of any memory resident object, as returned by sizeof.
 */
typedef unsigned int	Size;

/*
 * Index --
 *	Index into any memory resident array.
 *
 * Note:
 *	Indices are non negative.
 */
typedef unsigned int	Index;

#define MAXDIM 6
typedef struct {
	int indx[MAXDIM];
} IntArray;

/*
 * Count --
 *	Generic counter type.
 */
typedef unsigned int	Count;

/*
 * Offset --
 *	Offset into any memory resident array.
 *
 * Note:
 *	This differs from an Index in that an Index is always
 *	non negative, whereas Offset may be negative.
 */
typedef signed int	Offset;

/*
 * String --
 *	String of characters.
 */
typedef char	*String;

typedef int	*intPtr;
/*
 * ReturnStatus --
 *	Return status from POSTGRES library functions.
 *
 * Note:
 *	Most POSTGRES functions will not return a status.
 *	In the future, there should be a global variable
 *	which indicates the reason for the failure--the
 *	identifier of an error message in the ERROR relation.
 */
typedef int	ReturnStatus;

/* ----------------------------------------------------------------
 *		Section 5:  malloc, free, malloc_debug stuff
 *			    new, delete macros
 * ----------------------------------------------------------------
 */
#ifdef PALLOC_DEBUG
extern
char *	/* as defined in /usr/lib/lint/llib-lc */
malloc_debug ARGS((
	String	file,
	int 	line,
	Size	nBytes
));
extern
/* void */      /* as defined in /usr/lib/lint/llib-lc */
free_debug ARGS((
	String	file,
	int	line,
        char    *p
));
#endif /* PALLOC_DEBUG */

/*
 * newv --
 *	Allocate a new array.
 */
#define newv(type, n)	LintCast(type *, malloc(sizeof (type) * (n)))

/*
 * delete --
 *	Free allocated storage.
 *
 * Note:
 *	The variable is set to null to help catch errors.
 */
#define delete(pointer)	(free((char *) (pointer)), (pointer) = NULL)

/* 
 * ALLOCATE() macro
 */
#define ALLOCATE(foo) (foo) palloc(sizeof(struct CppConcat(_,foo)))

/* ----------------------------------------------------------------
 *		Section 6:  IsValid macros for system types
 * ----------------------------------------------------------------
 */
/*
 * BoolIsValid --
 *	True iff bool is valid.
 */
#define	BoolIsValid(boolean)	((boolean) == false || (boolean) == true)

#define boolIsValid(b) \
    ((bool) ((b) == false || (b) == true))

/*
 * PointerIsValid --
 *	True iff pointer is valid.
 */
#define PointerIsValid(pointer)	(bool)((Pointer)(pointer) != NULL)

/*
 * PointerIsInBounds --
 *	True iff pointer is within given bounds.
 *
 * Note:
 *	Assumes the bounded interval to be [min,max),
 *	i.e. closed on the left and open on the right.
 */
#define PointerIsInBounds(pointer, min, max) \
	((min) <= (pointer) && (pointer) < (max))

/*
 * PointerIsAligned --
 *	True iff pointer is properly aligned to point to the given type.
 */
#define PointerIsAligned(pointer, type)	\
	(((long)(pointer) % (sizeof (type))) == 0)

/*
 * SizeIsValid --
 *	True iff size is valid.
 *
 * Note:
 *	Assumes that Size is an unsigned type.
 *
 *	Assumes valid size to be in the interval (0,infinity).
 */
#define SizeIsValid(size)		((size) > 0)

/*
 * SizeIsInBounds --
 *	True iff size is within given bounds.
 *
 * Note:
 *	Assumes Size is an unsigned type.
 *
 *	Assumes the bounded interval to be (0,max].
 */
#define SizeIsInBounds(size, max)	(0 < (size) && (size) <= (max))

/*
 * IndexIsValid --
 *	True iff index is valid.
 *
 * Note:
 *	Assumes Index is an unsigned type.
 *
 *	Assumes valid index to be in the interval [0,infinity).
 */
#define IndexIsValid(index)	true

/*
 * IndexIsInBounds --
 *	True iff index is within given bounds.
 *
 * Note:
 *	Assumes Index is an unsigned type.
 *
 *	Assumes the bounded interval to be [0,max).
 */
#define IndexIsInBounds(index, max)	((index) < (max))

/*
 * CountIsValid --
 *	True iff count is valid
 *
 * Note:
 *	Assumes Count is an unsigned type.
 *
 *	Assumes valid counts to be in the interval [0,infinity).
 */
#define CountIsValid(count)		true

/*
 * CountIsInBounds --
 *	True iff count is within given bounds.
 *
 * Note:
 *	Assumes Count is an unsigned type.
 *
 *	Assumes the bounded interval to be [0,max).
 */
#define CountIsInBounds(count, max)	IndexIsInBounds(count, max)

/*
 * OffsetIsInBounds --
 *	True iff offset is within given bounds.
 *
 * Note:
 *	Assumes the bounded interval to be [0,max).
 */
#define OffsetIsInBounds(offset, min, max) \
	((min) <= (offset) && (offset) < (max))

/*
 * StringIsValid --
 *	True iff string is valid.
 */
#define	StringIsValid(string)	PointerIsValid(string)

/*
 * ReturnStatusIsValid --
 *	True iff return status is valid.
 *
 * Note:
 *	Assumes that a library function can only indicate
 *	sucess or failure.
 */
#define ReturnStatusIsValid(status) \
	((-1) <= (status) && (status) <= 0)

/*
 * ReturnStatusIsSucess --
 *	True iff return status indicates a sucessful call.
 */
#define SucessfulReturnStatus(status) \
	((status) >= 0)

/*
 * End COMPILER AND HARDWARE DEPENDENT section
 */

/*
 * Begin COMPILER AND MACHINE INDEPENDENT section
 */
/* ----------------------------------------------------------------
 *		Section 7:  offsetof, lengthof, endof
 * ----------------------------------------------------------------
 */
/*
 * offsetof --
 *	Offset of a structure/union field within that structure/union.
 *
 *	XXX This is supposed to be part of stddef.h, but isn't on
 *	some systems (like SunOS 4).
 */
#ifndef offsetof
#define offsetof(type, field)	((long) &((type *)0)->field)
#endif /* offsetof */

/*
 * lengthof --
 *	Number of elements in an array.
 */
#define lengthof(array)	(sizeof (array) / sizeof ((array)[0]))

/*
 * endof --
 *	Address of the element one past the last in an array.
 */
#define endof(array)	(&array[lengthof(array)])

/* ----------------------------------------------------------------
 *		Section 8:  exception handling definitions
 *			    Assert, Trap, etc macros
 * ----------------------------------------------------------------
 */
/*
 * Exception Handling definitions
 */

typedef String	ExcMessage;
typedef struct Exception {
	ExcMessage	message;
} Exception;

extern Exception	FailedAssertion;
extern Exception	BadArg;
extern Exception	BadState;

extern
int
ExceptionalCondition ARGS((
	const String	conditionName,
	const Exception	*exceptionP,
	const String	details,
	const String	fileName,
	int		lineNumber
));

/*
 * NO_ASSERT_CHECKING, if defined, turns off all the assertions.
 * - plai  9/5/90
 *
 * It should _NOT_ be undef'ed in releases or in benchmark copies
 * 
 * #undef NO_ASSERT_CHECKING
 */

/*
 * Trap --
 *	Generates an exception if the given condition is true.
 *
 * Note:
 *	The trailing else is used to allow this macro to be used in
 *	single statement if, while, for, and do expressions, e.g.
 *
 *		if (tapeCount < 3)
 *			Trap(fd == -1, FailedFileOpen);
 *
 *		if (requests > 0)
 *			Assert(!QueueIsEmpty(queue));
 *
 *		if (!beenHere)
 *			AssertArg(PointerIsValid(initP));
 *
 *		if (beenHere)
 *			AssertState(ModuleInitialized)
 */
#define Trap(condition, exception) \
	if (condition) \
		ExceptionalCondition(CppAsString(condition), &(exception), \
			(String)NULL, __FILE__, __LINE__); \
	else

/*    
 *  TrapMacro is the same as Trap but it's intended for use in macros:
 *
 *	#define foo(x) (AssertM(x != 0) && bar(x))
 *
 *  Isn't CPP fun?
 */
#define TrapMacro(condition, exception) \
    ((bool) ((! condition) || \
	     (ExceptionalCondition(CppAsString(condition), \
				  &(exception), \
				  (String) NULL, __FILE__, __LINE__))))
    
#ifdef NO_ASSERT_CHECKING
#define Assert(condition)
#define AssertMacro(condition)	true
#define AssertArg(condition)
#define AssertState(condition)
#else
#define Assert(condition) \
	Trap(!(condition), FailedAssertion)

#define AssertMacro(condition) \
	TrapMacro(!(condition), FailedAssertion)

#define AssertArg(condition) \
	Trap(!(condition), BadArg)

#define AssertState(condition) \
	Trap(!(condition), BadState)

#endif   /* NO_ASSERT_CHECKING */

/*
 * LogTrap --
 *	Generates an exception with a message if the given condition is true.
 *
 * Note:
 *	The trailing else is used to allow this macro to be used in
 *	single statement if, while, for, and do expressions, e.g.
 *
 *		if (slowPointer)
 *			LogTrap(!PointerIsValid(pointer), NoMoreMemory,
 *				("size 0x%x", size));
 *
 *		if (requests > 0)
 *			LogAssert(!QueueIsEmpty(queue), ("req. %d", requests));
 *
 *		if (PointerIsSlowPointer(pointer))
 *			LogAssertArg(pointer >= &end, ("ptr 0x%x", pointer));
 *
 *		if (!slowPointer)
 *			LogAssertState(IsStackable(Current),
 *				("state %d", typeof(current)))
 */
#define LogTrap(condition, exception, printArgs) \
	if (condition) \
		ExceptionalCondition(CppAsString(condition), &(exception), \
			form printArgs, __FILE__, __LINE__); \
	else

/*    
 *  LogTrapMacro is the same as LogTrap but it's intended for use in macros:
 *
 *	#define foo(x) (LogAssertMacro(x != 0, "yow!") && bar(x))
 */
#define LogTrapMacro(condition, exception, printArgs) \
    ((bool) ((! condition) || \
	     (ExceptionalCondition(CppAsString(condition), \
				   &(exception), \
				   form printArgs, __FILE__, __LINE__))))
    
#ifdef NO_ASSERT_CHECKING
#define LogAssert(condition, printArgs)
#define LogAssertMacro(condition, printArgs) true
#define LogAssertArg(condition, printArgs)
#define LogAssertState(condition, printArgs)
#else
#define LogAssert(condition, printArgs) \
	LogTrap(!(condition), FailedAssertion, printArgs)

#define LogAssertMacro(condition, printArgs) \
	LogTrapMacro(!(condition), FailedAssertion, printArgs)

#define LogAssertArg(condition, printArgs) \
	LogTrap(!(condition), BadArg, printArgs)

#define LogAssertState(condition, printArgs) \
	LogTrap(!(condition), BadState, printArgs)

#endif   /* NO_ASSERT_CHECKING */

/* ----------------------------------------------------------------
 *		Section 9:  Min, Max, Abs macros
 * ----------------------------------------------------------------
 */
/*
 * Max --
 *	Return the maximum of two numbers.
 */
#define Max(x, y)	((x) > (y) ? (x) : (y))
#ifndef MAX
#define MAX(x, y)	((x) > (y) ? (x) : (y))
#endif /* MAX */

/*
 * Min --
 *	Return the minimum of two numbers.
 */
#define Min(x, y)	((x) < (y) ? (x) : (y))
#ifndef MIN
#define MIN(x, y)	((x) < (y) ? (x) : (y))
#endif /* MIN */

/*
 * Abs --
 *	Return the absolute value of the argument.
 */
#define Abs(x)		((x) >= 0 ? (x) : -(x))

/* ----------------------------------------------------------------
 *		Section 10:  LintCast, RevisionId, RcsId, SccsId macros
 * ----------------------------------------------------------------
 */
/*
 * LintCast --
 *	Coerce value to a given type, without upsetting lint.
 *	Use *ONLY* for *VALID* casts that lint complains about.
 *
 * Note:
 *	The ?: operator is used to avoid "variable unused" warnings.
 */
#ifdef	lint
# define LintCast(type, value)	((value) ? ((type) 0) : ((type) 0))

# define RevisionId(var, value)
#else	/* !defined(lint) */
# define LintCast(type, value)	((type) (value))

# ifdef XSTR
#  define RevisionId(var, value)	static char *var = value
# else	/* !defined(XSTR) */
#  define RevisionId(var, value)	static char var[] = value
# endif	/* !defined(XSTR) */
#endif	/* !defined(lint) */

/* ----------------
 *	RcsId and SccsId macros..
 * ----------------
 */
#if (defined(lint) || defined(SABER))
#define SccsId(id)	
#define RcsId(id)	
#else
#define SccsId(id)	RevisionId(_SccsId_, id)
#define RcsId(id)	RevisionId(_RcsId_, id)
#endif

/* ----------------------------------------------------------------
 *		Section 12: externs
 * ----------------------------------------------------------------
 */

/* ----------------
 *	form is used by assert and the exception handling stuff
 * ----------------
 */
extern String form();

/* ----------------------------------------------------------------
 *		Section 13: system-specific hacks
 *
 *	This should be limited to things that absolutely have to be
 *	included in every source file.  The changes should be factored
 *	into a separate file so that changes to one port don't require
 *	changes to c.h (and everyone recompiling their whole system).
 * ----------------------------------------------------------------
 */

#if defined(PORTNAME_hpux) 
#include "port/hpux/fixade.h"		/* for 8.07 unaligned access fixup */
#endif /* PORTNAME_hpux */

#if defined(PORTNAME_sparc_solaris) 
#include "port/sparc_solaris/strings.h"	/* b{copy,zero,cmp} is pervasive */
#endif /* PORTNAME_sparc_solaris */

/* ----------------
 *	end of c.h
 * ----------------
 */
#endif	/* !defined(CIncluded) */
