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

/*--------------------------------------------------------
 * SOME NOT VERY PORTABLE ROUTINES ???
 *--------------------------------------------------------
 *
 * In the implementation of the next routines we assume the following:
 *
 * A) if a type is "byVal" then all the information is stored in the
 * Datum itself (i.e. no pointers involved!). In this case the
 * length of the type is always greater than zero and less than
 * "sizeof(Datum)"
 * B) if a type is not "byVal" and it has a fixed length, then
 * the "Datum" always contain a pointer to a stream of bytes.
 * The number of significant bytes are always equal to the length of thr
 * type.
 * C) if a type is not "byVal" and is of variable length (i.e. it has
 * length == -1) then "Datum" always points to a "struct varlena".
 * This varlena structure has information about the actual length of this
 * particular instance of the type and about its value.
 */

/*---------------
 * datumGetRealLengthAndByVal
 * find the "real" length of a type
 */
extern
void
datumGetRealLengthAndByVal ARGS((
    Datum	value;
    ObjectId	type;
    Size	*sizeP;
    bool	*byValP;
));

/*---------------
 * copyDatum
 * make a copy of a datum.
 */
extern
Datum
copyDatum ARGS((
    Datum	value,
    ObjectId	type
));

/*---------------
 * freeDatum
 * free space that *might* have been palloced by "copyDatum"
 */
extern
void
freeDatum  ARGS((
    Datum	value,
    ObjectId	type
));


#endif	/* !defined(DatumIncluded) */
