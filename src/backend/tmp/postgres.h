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
 *   TABLE OF CONTENTS
 *	1)	simple type definitions
 *	1a)	oid types (currently being reorganized)
 *	2)	array types
 *	3)	name type + support macros	\
 *	4)	datum type + support macros	 \ being reorganized
 *	5)	xid type + support macros	 /
 *	6)	time types + support macros	/
 *	7)	genbki macros used by catalog/pg_xxx.h files
 *	8)	old types being obsoleted
 *	9)	random SIGNBIT, PSIZE, MAXPGPATH, STATUS macros
 *
 *   IDENTIFICATION
 *   	$Header$
 * ----------------------------------------------------------------
 */

#ifndef PostgresHIncluded		/* include only once */
#define PostgresHIncluded

#include "tmp/c.h"

/* ----------------------------------------------------------------
 *		Section 1:  simple type definitions
 * ----------------------------------------------------------------
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
typedef int32	dt;

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

/* ----------------------------------------------------------------
 *		Section 1a:  oid types
 * ----------------------------------------------------------------
 */
/* ----------------
 *	oid
 * ----------------
 */
typedef uint32	oid;

#define ObjectId oid
#define InvalidObjectId	0

#define ObjectIdIsValid(objectId) \
    ((bool) (objectId != InvalidObjectId))

/* ----------------
 *	regproc
 * ----------------
 */
typedef oid regproc;

typedef ObjectId RegProcedure;

#define RegProcedureIsValid(p) \
    ObjectIdIsValid(p)

/* ----------------------------------------------------------------
 *		Section 2:  array types
 * ----------------------------------------------------------------
 */
/* ----------------
 *	struct varlena
 * ----------------
 */
struct varlena {
	int32	vl_len;
	char	vl_dat[1];
};

#define	VARSIZE(PTR)	(((struct varlena *)(PTR))->vl_len)
#define	VARDATA(PTR)    (((struct varlena *)(PTR))->vl_dat)

/* ----------------
 *	aclitem
 * ----------------
 */
typedef int4 aclitem;

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

/* ----------------
 *	oidint4
 *
 *	this is a new system type used by the file interface.
 * ----------------
 */
typedef struct OidInt4Data {
	ObjectId	oi_oid;
	int32		oi_int4;
} OidInt4Data;

typedef struct OidInt4Data	*OidInt4;

/* ----------------
 *	oidchar16
 *
 *	this is a new system type used to define indices on two attrs.
 * ----------------
 */
typedef struct OidChar16Data {
	ObjectId	id;
	char16		name;
} OidChar16Data;

typedef struct OidChar16Data	*OidChar16;

/* ----------------------------------------------------------------
 *		Section 3:  name type + support macros
 * ----------------------------------------------------------------
 */
/* ----------------
 *	name
 *
 *     	definition of the catalog/system "name" data type.
 *	This is used by some of the access method and catalog
 *	support code.
 * ----------------
 */
typedef char16		NameData;
typedef NameData	*Name;

#define InvalidName	((Name) NULL)
#define	NameIsValid(name) PointerIsValid(name)

/* ----------------------------------------------------------------
 *	 	Section 4:  datum type + support macros
 * ----------------------------------------------------------------
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
 *  sizeof(Datum) == sizeof(long) >= sizeof(void *) >= 4
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

/* XXX wrong place */
#ifdef PORTNAME_alpha
#define	LONG64
#endif

/*
 * Identification:
 */

typedef struct AnyStruct {
	char    character;
	double  largeFloat;
} AnyStruct;

typedef unsigned long Datum;	/* XXX sizeof(long) >= sizeof(void *) */
typedef Datum *       DatumPtr;

#define GET_1_BYTE(datum)   (((Datum) (datum)) & 0x000000ff)
#define GET_2_BYTES(datum)  (((Datum) (datum)) & 0x0000ffff)
#define GET_4_BYTES(datum)  (((Datum) (datum)) & 0xffffffff)
#define SET_1_BYTE(value)   (((Datum) (value)) & 0x000000ff)
#define SET_2_BYTES(value)  (((Datum) (value)) & 0x0000ffff)
#define SET_4_BYTES(datum)  (((Datum) (datum)) & 0xffffffff)

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

#ifndef LONG64
#define DatumGetPointer(X) ((Pointer) GET_4_BYTES(X))
#else /* LONG64 */
#define DatumGetPointer(X) ((Pointer) X)
#endif /* LONG64 */

/*
 * PointerGetDatum --
 *	Returns datum representation for a pointer.
 */

#ifndef LONG64
#define PointerGetDatum(X) ((Datum) SET_4_BYTES(X))
#else /* LONG64 */
#define PointerGetDatum(X) ((Datum) X)
#endif /* LONG64 */

/*
 * DatumGetPointerPointer --
 *	Returns pointer to pointer value of a datum.
 */

#ifndef LONG64
#define DatumGetPointerPointer(X) ((Pointer *) GET_4_BYTES(X))
#else /* LONG64 */
#define DatumGetPointerPointer(X) ((Pointer *) X)
#endif /* LONG64 */

/*
 * PointerPointerGetDatum --
 *	Returns datum representation for a pointer to pointer.
 */

#ifndef LONG64
#define PointerPointerGetDatum(X) ((Datum) SET_4_BYTES(X))
#else /* LONG64 */
#define PointerPointerGetDatum(X) ((Datum) X)
#endif /* LONG64 */

/*
 * DatumGetStructPointer --
 *	Returns pointer to structure value of a datum.
 */

#ifndef LONG64
#define DatumGetStructPointer(X) ((AnyStruct *) GET_4_BYTES(X))
#else /* LONG64 */
#define DatumGetStructPointer(X) ((AnyStruct *) X)
#endif /* LONG64 */

/*
 * StructPointerGetDatum --
 *	Returns datum representation for a pointer to structure.
 */

#ifndef LONG64
#define StructPointerGetDatum(X) ((Datum) SET_4_BYTES(X))
#else /* LONG64 */
#define StructPointerGetDatum(X) ((Datum) X)
#endif /* LONG64 */

/*
 * DatumGetName --
 *	Returns name value of a datum.
 */

#ifndef LONG64
#define DatumGetName(X) ((Name) GET_4_BYTES(X))
#else /* LONG64 */
#define DatumGetName(X) ((Name) X)
#endif /* LONG64 */

/*
 * NameGetDatum --
 *	Returns datum representation for a name.
 */

#ifndef LONG64
#define NameGetDatum(X) ((Datum) SET_4_BYTES(X))
#else /* LONG64 */
#define NameGetDatum(X) ((Datum) X)
#endif /* LONG64 */

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


/* ----------------------------------------------------------------
 *		Section 5: xid type + support macros
 * ----------------------------------------------------------------
 */
/* 
 *	TransactionId definition
 */

typedef uint32			TransactionId;
#define TransactionIdDataSize	sizeof(TransactionId)

#define InvalidTransactionId	0
#define NullTransactionIdValue	0

typedef uint32			TransactionIdValueData;
typedef uint32			TransactionIdValue;

typedef uint16			CommandId;

#define FirstCommandId	0

#define TransactionMultiplierPerByte	(1 << BitsPerByte)
#define TransactionsPerSecondAdjustment	TransactionMultiplierPerByte	

/* ----------------------------------------------------------------
 *		Section 6: some time types + support macros
 *
 *	Note: see nabstime.h for time support macros
 * ----------------------------------------------------------------
 */

typedef uint32	Time;			/* XXX this will disappear */

typedef uint32	RelativeTime;
#define InvalidRelativeTime	0

/* ----------------------------------------------------------------
 *		Section 7: genbki macros used by the
 *			   catalog/pg_xxx.h files
 * ----------------------------------------------------------------
 */
#define CATALOG(x) \
    typedef struct CppConcat(FormData_,x)

#define DATA(x)
#define DECLARE_INDEX(x)
#define BUILD_INDICES
#define BOOTSTRAP

#define BKI_BEGIN
#define BKI_END

/* ----------------------------------------------------------------
 *		Section 8: old types being obsoleted
 * ----------------------------------------------------------------
 */
typedef	int32	XID;
#define	CID	uint16
#define	ABSTIME	uint32
#define	RELTIME	uint32

#define OID	oid
#define	REGPROC	oid		/* for now */
typedef	char	*DATUM;


/* ----------------------------------------------------------------
 *		Section 9:  random stuff
 *			    SIGNBIT, PSIZE, MAXPGPATH, STATUS...
 * ----------------------------------------------------------------
 */

/* msb for int/unsigned */
#define	SIGNBIT	(0x8000)

/* msb for char */
#define	CSIGNBIT (1 << 7)

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
#define STATUS_NOT_DONE		(-6)
#define STATUS_FOUND            (1)

/*
 * The following is a gimmick to pass functions that return void as
 * paramters to other functions.  This type defined here is used as
 * the cast.
 */
typedef Pointer (*VoidFunctionType)();

/* ----------------------------------------------------------------
 *	externs
 *
 *	This should be eliminated or moved elsewhere
 * ----------------------------------------------------------------
 */
extern bool NameIsEqual ARGS((Name name1, Name name2));
extern uint32 NameComputeLength ARGS((Name name));

extern TransactionId	NullTransactionId;
extern TransactionId	AmiTransactionId;
extern TransactionId	FirstTransactionId;

extern bool TransactionIdIsValid ARGS((TransactionId transactionId));
extern void GetNewTransactionId ARGS((TransactionId *xid));
extern TransactionId StringFormTransactionId ARGS((String representation));
extern String TransactionIdFormString ARGS((TransactionId transactionId));
extern void TransactionIdStore
    ARGS((TransactionId	transactionId, TransactionId *destination));
extern void PointerStoreInvalidTransactionId ARGS((Pointer destination));
extern bool TransactionIdEquals ARGS((TransactionId id1,TransactionId id2));
extern bool TransactionIdIsLessThan 
    ARGS((TransactionId	id1, TransactionId id2));
extern bool TransactionIdValueIsValid ARGS((TransactionIdValue value));

extern void TransactionIdIncrement ARGS((TransactionId *transactionId));
    
/* ----------------
 *	end of postgres.h
 * ----------------
 */
#endif PostgresHIncluded
