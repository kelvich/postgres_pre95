/*
 * c.h --
 *	Fundamental C definitions.
 *
 * Note:
 *	This file is MACHINE AND COMPILER dependent!!!  (For now.)
 *
 * Identification:
 *	$Header$
 */

#ifndef	CIncluded		/* Include this file only once */
#define CIncluded	1

#define C_H	"$Header$"

/*
 * Begin COMPILER DEPENDENT section
 */

/*
 * bool --
 *	Boolean value, either true or false.
 */
typedef enum bool {
	false,	/* must be first, to be 0 */
	true
} bool;

/*
 * CppIdentity --
 *	C preprocessor identity macro, returns the argument.
 *	Used in non ANSI C compilers to concatenate tokens.
 */
#define CppIdentity(x)x

#ifdef	__STDC__ /* ANSI C */

/*
 * CppAsString --
 *	Convert the argument to a string, using the C preprocessor.
 */
#define CppAsString(identifier)	#identifier

/*
 * CppConcat --
 *	Concatenate two arguments together, using the C preprocessor.
 */
#define CppConcat(x, y)		x##y

/*
 * ARGS --
 *	Specifies parameter types of the declared function.
 *
 * Example:
 *	extern int	printf ARGS((const String format, ...));
 *	extern void	noop ARGS((void));
 */
#define ARGS(args)	args

/*
 * Pointer --
 *	Variable holding address of any memory resident object.
 */
typedef void	*Pointer;

#ifndef	NULL
/*
 * NULL --
 *	Null pointer.
 */
#define NULL	((void *) 0)
#endif	/* !defined(NULL) */

#else	/* !defined(__STDC__) */ /* NOT ANSI C */

/*
 * CppAsString --
 *	Convert the argument to a string, using the C preprocessor.
 */
#define CppAsString(identifier)	"identifier"

/*
 * CppConcat --
 *	Concatenate two arguments together, using the C preprocessor.
 */
#define CppConcat(x, y)		CppIdentity(x)y

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

/*
 * ARGS --
 *	Specifies parameter types of the declared function.
 *
 * Example:
 *	extern int	printf ARGS((const String format, ...));
 */
#define ARGS(args)	(/* args */)

/*
 * offsetof --
 *	Offset of a structure/union field within that structure/union.
 */
#define offsetof(type, field)	((long) &((type *)0)->field)

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

#endif	/* !defined(__STDC__) */ /* NOT ANSI C */


#ifndef __GNUC__	/* GNU cc */
# define inline
#endif


/*
 * BoolIsValid --
 *	True iff bool is valid.
 */
#define	BoolIsValid(boolean)	((boolean) == false || (boolean) == true)

/*
 * PointerIsValid --
 *	True iff pointer is valid.
 */
#define PointerIsValid(pointer)	(bool)((pointer) != NULL)

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
typedef signed long	int32;		/* >= 32 bits */

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
typedef unsigned long	uint32;		/* >= 32 bits */

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
 * Index --
 *	Index into any memory resident array.
 *
 * Note:
 *	Indices are non negative.
 */
typedef unsigned int	Index;

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
 * Count --
 *	Generic counter type.
 */
typedef unsigned int	Count;

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
 * Offset --
 *	Offset into any memory resident array.
 *
 * Note:
 *	This differs from an Index in that an Index is always
 *	non negative, whereas Offset may be negative.
 */
typedef signed int	Offset;

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
 * End COMPILER AND HARDWARE DEPENDENT section
 */


/*
 * Begin COMPILER AND MACHINE INDEPENDENT section
 */

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

extern
char *	/* as defined in /usr/lib/lint/llib-lc */
malloc ARGS((
	Size	nBytes
));

/*
 * new --
 *	Allocate a new instance of the given type.
 *
 * Note:
 *	Does NOT work with arrays.  Use newv instead.
 */
#define new(type)	LintCast(type *, malloc(sizeof (type)))

/*
 * newv --
 *	Allocate a new array.
 */
#define newv(type, n)	LintCast(type *, malloc(sizeof (type) * (n)))

extern
/* void */	/* as defined in /usr/lib/lint/llib-lc */
free ARGS((
	char	*p
));

/*
 * delete --
 *	Free allocated storage.
 *
 * Note:
 *	The variable is set to null to help catch errors.
 */
#define delete(pointer)	(free((char *) (pointer)), (pointer) = NULL)

/*
 * String --
 *	String of characters.
 */
typedef char	*String;

/*
 * StringIsValid --
 *	True iff string is valid.
 */
#define	StringIsValid(string)	PointerIsValid(string)

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
void
ExceptionalCondition ARGS((
	const String	conditionName,
	const Exception	*exceptionP,
	const String	details,
	const String	fileName,
	int		lineNumber
));

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

#define Assert(condition) \
	Trap(!(condition), FailedAssertion)

#define AssertArg(condition) \
	Trap(!(condition), BadArg)

#define AssertState(condition) \
	Trap(!(condition), BadState)

extern
String
form ARGS((
	String	format,...
));

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

#define LogAssert(condition, printArgs) \
	LogTrap(!(condition), FailedAssertion, printArgs)

#define LogAssertArg(condition, printArgs) \
	LogTrap(!(condition), BadArg, printArgs)

#define LogAssertState(condition, printArgs) \
	LogTrap(!(condition), BadState, printArgs)

/*
 * Max --
 *	Return the maximum of two numbers.
 */
#define Max(x, y)	((x) > (y) ? (x) : (y))

/*
 * Min --
 *	Return the minimum of two numbers.
 */
#define Min(x, y)	((x) < (y) ? (x) : (y))

/*
 * Abs --
 *	Return the absolute value of the argument.
 */
#define Abs(x)		((x) >= 0 ? (x) : -(x))

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
#ifdef LINT
#define SccsId(id)	
#define RcsId(id)	
#else
#define SccsId(id)	RevisionId(_SccsId_, id)
#define RcsId(id)	RevisionId(_RcsId_, id)
#endif

#define ALLOCATE(foo) (foo)palloc(sizeof(struct CppConcat(_,foo)))

#endif	/* !defined(CIncluded) */
