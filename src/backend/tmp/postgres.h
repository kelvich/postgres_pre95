/* ----------------------------------------------------------------
 *   FILE
 *     	postgres.h
 *
 *   DESCRIPTION
 *     	definition of (and support for) postgres system types.
 *
 *   NOTES
 *	this file will eventually contain the definitions for the
 *	following (and perhaps other) system types:
 *
 *		bool		bytea		char		
 *		char16		dt		int2
 *		int28		int4		oid		
 *		oid8		regproc		text
 *
 *	Currently this file is also the home of struct varlena and
 *	some other stuff.  Some of this may change.  -cim 8/6/90
 *
 *   IDENTIFICATION
 *   	$Header$
 * ----------------------------------------------------------------
 */

#ifndef PostgresHIncluded		/* include only once */
#define PostgresHIncluded

#include "tmp/c.h"

/* --------------------------------
 *	simple types
 * --------------------------------
 */
/* ----------------
 *	bool
 *
 *	this is defined in c.h at present, but that will change soon.
 *	Boolean should be obsoleted.
 * ----------------
 */
/* XXX put bool here */
#define Boolean bool

/* ----------------
 *	dt
 * ----------------
 */
typedef long	dt;

/* ----------------
 *	int2
 * ----------------
 */
typedef	int16	int2;

/* ----------------
 *	int4
 * ----------------
 */
typedef int32	int4;

/* ----------------
 *	float4
 * ----------------
 */
typedef float	float4;

/* ----------------
 *	float8
 * ----------------
 */
typedef double	float8;

/* ----------------
 *	oid
 *
 *	(oid.h is being obsoleted -cim)
 * ----------------
 */
/*
 * oid.h --
 *	POSTGRES object identifier definitions.
 *
 * $Header$
 */

#ifndef	OIdIncluded		/* Include this file only once */
#define OIdIncluded	1

typedef uint32	oid;

#define ObjectId oid

#define InvalidObjectId	0

/*
 * ObjectIdIsValid
 *	True iff the object identifier is valid.
 */
extern
bool
ObjectIdIsValid ARGS((
	ObjectId	objectId
));

#endif	/* !defined(OIdIncluded) */

/* ----------------
 *	regproc
 *
 *	(regproc.h is being obsoleted)
 * ----------------
 */
typedef oid regproc;

/*
 * regproc.h --
 *	POSTGRES registered procedure definitions.
 */

#ifndef	RegProcIncluded		/* Include this file only once */
#define RegProcIncluded	1

/*
 * Identification:
 */
#define REGPROC_H	"$Header$"

typedef ObjectId	RegProcedure;

/*
 * RegProcedureIsValid
 *	True iff the registered procedure is valid.
 */
extern
bool
RegProcedureIsValid ARGS((
	RegProcedure	proc
));

#endif	/* !defined(RegProcIncluded) */

/* --------------------------------
 *	array types
 * --------------------------------
 */
/* ----------------
 *	struct varlena
 * ----------------
 */
struct varlena {
	long	vl_len;
	char	vl_dat[1];
};

#define	VARSIZE(PTR)	(((struct varlena *)(PTR))->vl_len)
#define	VARDATA(PTR)    (((struct varlena *)(PTR))->vl_dat)

/* ----------------
 *	bytea
 * ----------------
 */
typedef struct varlena bytea;

/* ----------------
 *	char16
 * ----------------
 */
typedef struct char16 {
	char	data[16];
} char16;

typedef char16	*Char16;

/* ----------------
 *	int28
 * ----------------
 */
typedef struct int28 {
    int2	data[8];
} int28;

/* ----------------
 *	oid8
 * ----------------
 */
typedef struct oid8 {
    oid		data[8];
} oid8;
 
/* ----------------
 *	text
 * ----------------
 */
typedef struct varlena text;

/* ----------------
 *	stub
 *
 *	this is a new system type used by the rule manager.
 * ----------------
 */
typedef struct varlena stub;

/* ----------------------------------------------------------------
 *	other system types imported from:
 *
 *	name.h		system "name" data type
 *	datum.h		attribute values
 *	xid.h		transaction ids
 *	tim.h		absolute and relative time
 * ----------------------------------------------------------------
 */

/* ----------------
 *	name.h
 * ----------------
 */
/* 
 *   FILE
 *     	name.h
 *
 *   DESCRIPTION
 *     	definition of the catalog/system "name" data type.
 *	This is used by some of the access method and catalog
 *	support code.
 *
 *   NOTES
 *	Since "name" derives directly from a system type, this
 *	stuff should arguably go in postgres.h
 *
 *   IDENTIFICATION
 *   	$Header$
 * 
 */
#ifndef NameIncluded		/* Include this file only once */
#define	NameIncluded	1

/* 
 *	definition of NameData and Name
 *	Note: char16 is a system type in postgres.h
 * 
 */
typedef char16		NameData;
typedef NameData	*Name;

/* 
 *	Name support stuff.
 * 
 */
#define InvalidName	((Name)NULL)

/*
 * NameIsValid
 *	True iff the name is valid.
 */
extern
bool
NameIsValid ARGS((
	Name	name
));

/*
 * NameIsEqual
 *	True iff the names are the same.
 */
extern
bool
NameIsEqual ARGS((
	Name	name1,
	Name	name2
));

/*
 * NameComputeLength
 *	Returns the lenght of a name.
 */
uint32
NameComputeLength ARGS((
	Name	name
));

#endif	/* !defined(NameIncluded) */

/* ----------------
 *	datum.h
 * ----------------
 */
/*
 * datum.h --
 *	POSTGRES abstract data type datum representation definitions.
 *
 * Note:
 *
 * Port Notes:
 *  Postgres makes the following assumption about machines:
 *
 *  sizeof(Datum) == sizeof(char *) == sizeof(long) == 4
 *
 *  Postgres also assumes that
 *
 *  sizeof(char) == 1
 *
 *  and that 
 *
 *  sizeof(short) == 2
 *
 *  If your machine meets these requirements, Datums should also be checked
 *  to see if the positioning is correct.
 *
 *	This file is MACHINE AND COMPILER dependent!!!
 */

#ifndef	DatumIncluded		/* Include this file only once */
#define DatumIncluded	1

/*
 * Identification:
 */
#define DATUM_H	"$Header$"

typedef struct AnyStruct {
	char    character;
	double  largeFloat;
} AnyStruct;

typedef unsigned long Datum;

/*
 * We want to pad to the right on Sun computers and to the right on
 * the others.
 * 
 */

#ifdef NOTDEF

#define GET_1_BYTE(datum)   ((((long) (datum)) & 0xff000000) >> 24)
#define GET_2_BYTES(datum)  ((((long) (datum)) & 0xffff0000) >> 16)
#define GET_4_BYTES(datum)  (datum)
#define SET_1_BYTE(value)   (((long) (value)) << 24)
#define SET_2_BYTES(value)  (((long) (value)) << 16)
#define SET_4_BYTES(value)  (value)

#endif

#if defined(sequent) || defined(mips) || defined(sun) || defined(sparc)

#define GET_1_BYTE(datum)   (((Datum) (datum)) & 0x000000ff)
#define GET_2_BYTES(datum)  (((Datum) (datum)) & 0x0000ffff)
#define GET_4_BYTES(datum)  ((Datum) (datum))
#define SET_1_BYTE(value)   (((Datum) (value)) & 0x000000ff)
#define SET_2_BYTES(value)  (((Datum) (value)) & 0x0000ffff)
#define SET_4_BYTES(value)  ((Datum) (value))

#endif

/*
 * DatumGetChar --
 *	Returns character value of a datum.
 */

#define DatumGetChar(X) ((char) GET_1_BYTE(X))

/*
 * CharGetDatum --
 *	Returns datum representation for a character.
 */

#define CharGetDatum(X) ((Datum) SET_1_BYTE(X))

/*
 * DatumGetInt8 --
 *	Returns 8-bit integer value of a datum.
 */

#define DatumGetInt8(X) ((int8) GET_1_BYTE(X))

/*
 * Int8GetDatum --
 *	Returns datum representation for an 8-bit integer.
 */

#define Int8GetDatum(X) ((Datum) SET_1_BYTE(X))

/*
 * DatumGetUInt8 --
 *	Returns 8-bit unsigned integer value of a datum.
 */

#define DatumGetUInt8(X) ((uint8) GET_1_BYTE(X))

/*
 * UInt8GetDatum --
 *	Returns datum representation for an 8-bit unsigned integer.
 */

#define UInt8GetDatum(X) ((Datum) SET_1_BYTE(X))

/*
 * DatumGetInt16 --
 *	Returns 16-bit integer value of a datum.
 */

#define DatumGetInt16(X) ((int16) GET_2_BYTES(X))

/*
 * Int16GetDatum --
 *	Returns datum representation for a 16-bit integer.
 */

#define Int16GetDatum(X) ((Datum) SET_2_BYTES(X))

/*
 * DatumGetUInt16 --
 *	Returns 16-bit unsigned integer value of a datum.
 */

#define DatumGetUInt16(X) ((uint16) GET_2_BYTES(X))

/*
 * UInt16GetDatum --
 *	Returns datum representation for a 16-bit unsigned integer.
 */

#define UInt16GetDatum(X) ((Datum) SET_2_BYTES(X))

/*
 * DatumGetInt32 --
 *	Returns 32-bit integer value of a datum.
 */

#define DatumGetInt32(X) ((int32) GET_4_BYTES(X))

/*
 * Int32GetDatum --
 *	Returns datum representation for a 32-bit integer.
 */

#define Int32GetDatum(X) ((Datum) SET_4_BYTES(X))

/*
 * DatumGetUInt32 --
 *	Returns 32-bit unsigned integer value of a datum.
 */

#define DatumGetUInt32(X) ((uint32) GET_4_BYTES(X))

/*
 * UInt32GetDatum --
 *	Returns datum representation for a 32-bit unsigned integer.
 */

#define UInt32GetDatum(X) ((Datum) SET_4_BYTES(X))

/*
 * DatumGetFloat32 --
 *	Returns 32-bit floating point value of a datum.
 */

#define DatumGetFloat32(X) ((float32) GET_4_BYTES(X))

/*
 * Float32GetDatum --
 *	Returns datum representation for a 32-bit floating point number.
 */

#define Float32GetDatum(X) ((Datum) SET_4_BYTES(X))

/*
 * DatumGetFloat64 --
 *	Returns 64-bit floating point value of a datum.
 */

#define DatumGetFloat64(X) ((float64) GET_4_BYTES(X))

/*
 * Float64GetDatum --
 *	Returns datum representation for a 64-bit floating point number.
 */

#define Float64GetDatum(X) ((Datum) SET_4_BYTES(X))

/*
 * DatumGetPointer --
 *	Returns pointer value of a datum.
 */

#define DatumGetPointer(X) ((Pointer) GET_4_BYTES(X))

/*
 * PointerGetDatum --
 *	Returns datum representation for a pointer.
 */

#define PointerGetDatum(X) ((Datum) SET_4_BYTES(X))

/*
 * DatumGetPointerPointer --
 *	Returns pointer to pointer value of a datum.
 */

#define DatumGetPointerPointer(X) ((Pointer *) GET_4_BYTES(X))

/*
 * PointerPointerGetDatum --
 *	Returns datum representation for a pointer to pointer.
 */

#define PointerPointerGetDatum(X) ((Datum) SET_4_BYTES(X))

/*
 * DatumGetStructPointer --
 *	Returns pointer to structure value of a datum.
 */

#define DatumGetStructPointer(X) ((AnyStruct *) GET_4_BYTES(X))

/*
 * StructPointerGetDatum --
 *	Returns datum representation for a pointer to structure.
 */

#define StructPointerGetDatum(X) ((Datum) SET_4_BYTES(X))

/*
 * DatumGetName --
 *	Returns name value of a datum.
 */

#define DatumGetName(X) ((Name) GET_4_BYTES(X))

/*
 * NameGetDatum --
 *	Returns datum representation for a name.
 */

#define NameGetDatum(X) ((Datum) SET_4_BYTES(X))

/*
 * DatumGetObjectId --
 *	Returns object identifier value of a datum.
 */

#define DatumGetObjectId(X) ((ObjectId) GET_4_BYTES(X))

/*
 * ObjectIdGetDatum --
 *	Returns datum representation for an object identifier.
 */

#define ObjectIdGetDatum(X) ((Datum) SET_4_BYTES(X))

#endif	/* !defined(DatumIncluded) */

/* ----------------
 *	xid.h
 * ----------------
 */
/*
 * xid.h --
 *	POSTGRES transaction identifier definitions.
 */

#ifndef	XIdIncluded		/* Include this file only once */
#define XIdIncluded	1

/*
 * Identification:
 */
#define XID_H	"$Header$"

/* 
 *	TransactionId definition
 * 
 */

typedef struct TransactionIdData {
	uint8	data[5];
} TransactionIdData;

#define TransactionIdDataSize	5

typedef TransactionIdData	*TransactionId;

#define InvalidTransactionId	NULL
#define NullTransactionIdValue	0.0

typedef double			TransactionIdValueData;
typedef TransactionIdValueData	*TransactionIdValue;

typedef uint8	CommandId;

#define FirstCommandId	0

#define TransactionMultiplierPerByte	(1 << BitsPerByte)
#define TransactionsPerSecondAdjustment	TransactionMultiplierPerByte	

extern TransactionId	NullTransactionId;
extern TransactionId	AmiTransactionId;
extern TransactionId	FirstTransactionId;

/* 
 * TransactionIdIsValid --
 *	True iff transaction identifier is valid.
 * 
 */
extern
bool
TransactionIdIsValid ARGS((
	TransactionId	transactionId
));

/* 
 * GetNewTransactionId --
 *	Returns a new transaction identifier.
 * 
 */
extern
void
GetNewTransactionId ARGS((
	TransactionId   xid
));

/* 
 * StringFormTransactionId --
 *	Returns transaction identifier associated with a string.
 * 
 */
extern
TransactionId
StringFormTransactionId ARGS((
	String	representation
));

/* 
 * TransactionIdValueFormString --
 *	Returns string representation for a transaction identifier.
 * 
 */
extern
String
TransactionIdFormString ARGS((
	TransactionId	transactionId
));

/* 
 * TransactionIdStore --
 *	Stores the transaction identifier in external form.
 *
 * Note:
 *	Assumes transaction identifier is valid.
 *	Assumes destination pointer is valid.
 * 
 */
extern
void
TransactionIdStore ARGS((
	TransactionId	transactionId,
	Pointer		destination
));

/* 
 * PointerStoreInvalidTransactionId --
 *	Stores an invalid transaction identifier (in external form).
 *
 * Note:
 *	Assumes destination pointer is valid.
 * 
 */
extern
void
PointerStoreInvalidTransactionId ARGS((
	Pointer		destination
));

/* 
 * TransactionIdEquals --
 *	True iff transaction identifiers are equal.
 *
 * Note:
 *	Assumes transaction identifiers are valid.
 * 
 */
extern
bool
TransactionIdEquals ARGS((
	TransactionId	id1,
	TransactionId	id2
));

/* 
 * TransactionIdIsLessThan --
 *	True iff first transaction identifier is less than the second.
 *
 * Note:
 *	Assumes transaction identifiers are valid.
 * 
 */
extern
bool
TransactionIdIsLessThan ARGS((
	TransactionId	id1,
	TransactionId	id2
));
/* 
 * TransactionIdValueIsValid --
 *	True iff transaction identifier value is valid.
 * 
 */
extern
bool
TransactionIdValueIsValid ARGS((
	TransactionIdValue	value
));

/* 
 * StringGetTransactionIdValue --
 *	Returns transaction identifier value associated with a string.
 * 
 */
extern
void
StringSetTransactionIdValue ARGS((
	String			representation,
	TransactionIdValue	value
));

/* 
 * TransactionIdValueFormString --
 *	Returns string representation for a transaction identifier value.
 * 
 */
extern
String
TransactionIdValueFormString ARGS((
	TransactionIdValue	value
));

/* 
 * TransactionIdValueSetTransactionId --
 *	Sets transaction identifier to a transaction identifier value.
 * 
 */
extern
void
TransactionIdValueSetTransactionId ARGS((
	TransactionIdValue	idValue,
	TransactionId		id
));

/* 
 * TransactionIdSetTransactionIdValue --
 *	Sets transaction identifier value for a transaction identifier.
 * 
 */
extern
void
TransactionIdSetTransactionIdValue ARGS((
	TransactionId		id,
	TransactionIdValue	idValue
));

/* 
 * TransactionIdIncrement --
 *	Increments transaction identifier.
 * 
 */
extern
void
TransactionIdIncrement ARGS((
	TransactionId	transactionId
));

#endif	/* !defined(XIdIncluded) */

/* ----------------
 *	tim.h
 * ----------------
 */
/*
 * tim.h --
 *	POSTGRES time definitions.
 */

#ifndef	TimIncluded		/* Include this file only once */
#define TimIncluded	1

/*
 * Identification:
 */
#define TIM_H	"$Header$"


typedef uint32	AbsoluteTime;

#define InvalidAbsoluteTime	0

typedef uint32	RelativeTime;

#define InvalidRelativeTime	0

typedef uint32	Time;		/* XXX this will disappear */

#define InvalidTime	0	/* XXX this will disappear */

/*
 * XXX INVALID_ABSTIME from adt/date.h.
 * XXX access/tim and adt/date should be merged somehow.
 */
#ifndef	INVALID_ABSTIME
#define INVALID_ABSTIME	2147483647
#endif	/* !defined(INVALID_ABSTIME) */

/*
 * AbsoluteTimeIsValid --
 *	True iff absolute time is valid.
 */
extern
bool
AbsoluteTimeIsValid ARGS((
	AbsoluteTime	time
));

/*
 * RelativeTimeIsValid --
 *	True iff relative time is valid.
 */
extern
bool
RelativeTimeIsValid ARGS((
	AbsoluteTime	time
));

/*
 * GetCurrentAbsoluteTime --
 *	Returns the current absolute time.
 */
AbsoluteTime
GetCurrentAbsoluteTime ARGS((
	void
));

/*
 * AbsoluteTimeIsBefore --
 *	True iff an absolute time is before or the same as another.
 *
 * Note:
 *	Assumes absolute times are valid.
 */
bool
AbsoluteTimeIsBefore ARGS((
	AbsoluteTime	time1,
	AbsoluteTime	time2
));

/*
 * AbsoluteTimeIsAfter --
 *	True iff an absolute time is strictly after another.
 *
 * Note:
 *	Assumes absolute times are valid.
 */
bool
AbsoluteTimeIsAfter ARGS((
	AbsoluteTime	time1,
	AbsoluteTime	time2
));

#endif	/* !defined(TimIncluded) */

/* ----------------------------------------------------------------
 *	system catalog macros used by the catalog/pg_xxx.h files
 * ----------------------------------------------------------------
 */
#define CATALOG(x) \
    typedef struct CppConcat(FormData_,x)

#define DATA(x)
#define BOOTSTRAP

#define BKI_BEGIN
#define BKI_END

/* ----------------------------------------------------------------
 *	real old type names, may be obsoleted soon -cim 8/6/90
 * XXX These should disappear!!! -hirohama
 * ----------------------------------------------------------------
 */
typedef	char	XID[5];
#define	CID	unsigned char
#define	ABSTIME	long
#define	RELTIME	long
#define OID	oid
#define	REGPROC	oid		/* for now */

#if defined(sun) || defined(sequent) || defined(mips)
typedef	char	*DATUM;
#else
typedef	union {
	char	*da_cprt;
	short	*da_shprt;
	long	*da_lprt;
	char	**da_cpprt;
	struct	{
		char	s_c;
		double	s_d;
	}	*da_stptr;
	long	da_long;
}		DATUM;
#endif


/* ----------------
 *	random stuff
 * ----------------
 */
#define	SIGNBIT	(0x8000)	/* msb for int/unsigned */
#define	CSIGNBIT	(1 << 7)	/* msb for char */

/* ----------------
 *	PSIZE
 * ----------------
 */
#define	PSIZE(PTR)	(*((int32 *)(PTR) - 1))
#define	PSIZEALL(PTR)	(*((int32 *)(PTR) - 1) + sizeof (int32))
#define	PSIZESKIP(PTR)	((char *)((int32 *)(PTR) + 1))
#define	PSIZEFIND(PTR)	((char *)((int32 *)(PTR) - 1))
#define	PSIZESPACE(LEN)	((LEN) + sizeof (int32))

/* ----------------
 *	global variable which should probably go someplace else.
 * ----------------
 */
#define	MAXPGPATH	128

#define STATUS_OK               (0)
#define STATUS_ERROR            (-1)
#define STATUS_NOT_FOUND        (-2)
#define STATUS_INVALID          (-3)
#define STATUS_UNCATALOGUED     (-4)
#define STATUS_REPLACED         (-5)
#define STATUS_FOUND            (1)
/* ----------------
 *	end of postgres.h
 * ----------------
 */
#endif PostgresHIncluded
