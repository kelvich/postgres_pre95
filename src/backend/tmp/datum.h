
; /*
; * 
; * POSTGRES Data Base Management System
; * 
; * Copyright (c) 1988 Regents of the University of California
; * 
; * Permission to use, copy, modify, and distribute this software and its
; * documentation for educational, research, and non-profit purposes and
; * without fee is hereby granted, provided that the above copyright
; * notice appear in all copies and that both that copyright notice and
; * this permission notice appear in supporting documentation, and that
; * the name of the University of California not be used in advertising
; * or publicity pertaining to distribution of the software without
; * specific, written prior permission.  Permission to incorporate this
; * software into commercial products can be obtained from the Campus
; * Software Office, 295 Evans Hall, University of California, Berkeley,
; * Ca., 94720 provided only that the the requestor give the University
; * of California a free licence to any derived software for educational
; * and research purposes.  The University of California makes no
; * representations about the suitability of this software for any
; * purpose.  It is provided "as is" without express or implied warranty.
; * 
; */



/*
 * datum.h --
 *	POSTGRES abstract data type datum representation definitions.
 *
 * Note:
 *	This file is MACHINE AND COMPILER dependent!!!
 *
 * Identification:
 *	$Header$
 */

#ifndef	DatumIncluded	/* Include this file only once. */
#define DatumIncluded	1

#include "c.h"

#include "name.h"
#include "oid.h"

typedef struct AnyStructure {
	char    character;
	double  largeFloat;
} AnyStructure;

#ifdef	sun
typedef union Datum {
	struct character {
		char	filler[3];	
		char	value;
	} character;
	struct integer8 {
		int8	filler[3];
		int8	value;
	} integer8;
	struct unsignedInteger8 {
		uint8	filler[3];
		uint8	value;
	} unsignedInteger8;
	struct integer16 {
		int16	filler[1];
		int16	value;
	} integer16;
	struct unsignedInteger16 {
		uint16	filler[1];
		uint16	value;
	} unsignedInteger16;
	struct integer32 {
		int32	value;
	} integer32;
	struct unsignedInteger32 {
		uint32	value;
	} unsignedInteger32;
	struct smallFloat {
		float32	value;
	} smallFloat;
	struct largeFloat {
		float64	value;
	} largeFloat;
	struct pointer {
		Pointer	value;
	} pointer;
	struct pointerPointer {
		Pointer	*value;
	} pointerPointer;
	struct structurePointer {
		AnyStructure	*value;
	} structurePointer;
	struct name {
		Name	value;	
	} name;
	struct objectId {
		ObjectId	value;	
	} objectId;
} Datum;
#endif	/* defined(sun) */

/*
 * DatumGetChar --
 *	Returns character value of a datum.
 */
char
DatumGetChar ARGS((
	Datum	datum
));

/*
 * CharGetDatum --
 *	Returns datum representation for a character.
 */
Datum
CharGetDatum ARGS((
	char	character
));

/*
 * DatumGetInt8 --
 *	Returns 8-bit integer value of a datum.
 */
int8
DatumGetInt8 ARGS((
	Datum	datum
));

/*
 * Int8GetDatum --
 *	Returns datum representation for an 8-bit integer.
 */
Datum
Int8GetDatum ARGS((
	int8	integer8
));

/*
 * DatumGetUInt8 --
 *	Returns 8-bit unsigned integer value of a datum.
 */
uint8
DatumGetUInt8 ARGS((
	Datum	datum
));

/*
 * UInt8GetDatum --
 *	Returns datum representation for an 8-bit unsigned integer.
 */
Datum
UInt8GetDatum ARGS((
	uint8	unsignedInteger8
));

/*
 * DatumGetInt16 --
 *	Returns 16-bit integer value of a datum.
 */
int16
DatumGetInt16 ARGS((
	Datum	datum
));

/*
 * Int16GetDatum --
 *	Returns datum representation for a 16-bit integer.
 */
Datum
Int16GetDatum ARGS((
	int16	integer16
));

/*
 * DatumGetUInt16 --
 *	Returns 16-bit unsigned integer value of a datum.
 */
uint16
DatumGetUInt16 ARGS((
	Datum	datum
));

/*
 * UInt16GetDatum --
 *	Returns datum representation for a 16-bit unsigned integer.
 */
Datum
UInt16GetDatum ARGS((
	uint16	unsignedInteger16
));

/*
 * DatumGetInt32 --
 *	Returns 32-bit integer value of a datum.
 */
int32
DatumGetInt32 ARGS((
	Datum	datum
));

/*
 * Int32GetDatum --
 *	Returns datum representation for a 32-bit integer.
 */
Datum
Int32GetDatum ARGS((
	int32	integer32
));

/*
 * DatumGetUInt32 --
 *	Returns 32-bit unsigned integer value of a datum.
 */
uint32
DatumGetUInt32 ARGS((
	Datum	datum
));

/*
 * UInt32GetDatum --
 *	Returns datum representation for a 32-bit unsigned integer.
 */
Datum
UInt32GetDatum ARGS((
	uint32	unsignedInteger32
));

/*
 * DatumGetFloat32 --
 *	Returns 32-bit floating point value of a datum.
 */
float32
DatumGetFloat32 ARGS((
	Datum	datum
));

/*
 * Float32GetDatum --
 *	Returns datum representation for a 32-bit floating point number.
 */
Datum
Float32GetDatum ARGS((
	float32	smallFloat
));

/*
 * DatumGetFloat64 --
 *	Returns 64-bit floating point value of a datum.
 */
float64
DatumGetFloat64 ARGS((
	Datum	datum
));

/*
 * Float64GetDatum --
 *	Returns datum representation for a 64-bit floating point number.
 */
Datum
Float64GetDatum ARGS((
	float64	largeFloat
));

/*
 * DatumGetPointer --
 *	Returns pointer value of a datum.
 */
Pointer
DatumGetPointer ARGS((
	Datum	datum
));

/*
 * PointerGetDatum --
 *	Returns datum representation for a pointer.
 */
Datum
PointerGetDatum ARGS((
	Pointer	pointer
));

/*
 * DatumGetPointerPointer --
 *	Returns pointer to pointer value of a datum.
 */
Pointer *
DatumGetPointerPointer ARGS((
	Datum	datum
));

/*
 * PointerPointerGetDatum --
 *	Returns datum representation for a pointer to pointer.
 */
Datum
PointerPointerGetDatum ARGS((
	Pointer	*pointerPointerInP
));

/*
 * DatumGetStructPointer --
 *	Returns pointer to structure value of a datum.
 */
AnyStructure *
DatumGetStructPointer ARGS((
	Datum	datum
));

/*
 * StructPointerGetDatum --
 *	Returns datum representation for a pointer to structure.
 */
Datum
StructPointerGetDatum ARGS((
	AnyStructure	*structurePointerInP
));

/*
 * DatumGetName --
 *	Returns name value of a datum.
 */
Name
DatumGetName ARGS((
	Datum	datum
));

/*
 * NameGetDatum --
 *	Returns datum representation for a name.
 */
Datum
NameGetDatum ARGS((
	Name	name
));

/*
 * DatumGetObjectId --
 *	Returns object identifier value of a datum.
 */
ObjectId
DatumGetObjectId ARGS((
	Datum	datum
));

/*
 * ObjectIdGetDatum --
 *	Returns datum representation for an object identifier.
 */
Datum
ObjectIdGetDatum ARGS((
	ObjectId	objectId
));

#endif	/* !defined(DatumIncluded) */
