/*
 * datum.c --
 *	POSTGRES abstract data type datum representation code.
 */

#include "c.h"

RcsId("$Header$");

#include "datum.h"


/*
 * DatumGet --
 *	Datum accessor macro.
 */
#define DatumGet(datum, field)	(datum.field.value)

/*
 * DatumSet --
 *	Datum assignment macro.
 */
#define DatumSet(datum, field, newValue)	(datum.field.value) = newValue

char
DatumGetChar(datum)
	Datum	datum;
{
	return (DatumGet(datum, character));
}

Datum
CharGetDatum(character)
	char	character;
{
	Datum	datum;

	bzero(&datum, sizeof(Datum));
	DatumSet(datum, character, character);
	return (datum);
}

int8
DatumGetInt8(datum)
	Datum	datum;
{
	return (DatumGet(datum, integer8));
}

Datum
Int8GetDatum(integer8)
	int8	integer8;
{
	Datum	datum;

	bzero(&datum, sizeof(Datum));
	DatumSet(datum, integer8, AsInt8(integer8));
	return (datum);
}

uint8
DatumGetUInt8(datum)
	Datum	datum;
{
	return (DatumGet(datum, unsignedInteger8));
}

Datum
UInt8GetDatum(unsignedInteger8)
	uint8	unsignedInteger8;
{
	Datum	datum;

	bzero(&datum, sizeof(Datum));
	DatumSet(datum, unsignedInteger8, AsUint8(unsignedInteger8));
	return (datum);
}

int16
DatumGetInt16(datum)
	Datum	datum;
{
	return (DatumGet(datum, integer16));
}

Datum
Int16GetDatum(integer16)
	int16	integer16;
{
	Datum	datum;

	bzero(&datum, sizeof(Datum));
	DatumSet(datum, integer16, integer16);
	return (datum);
}

uint16
DatumGetUInt16(datum)
	Datum	datum;
{
	return (DatumGet(datum, unsignedInteger16));
}

Datum
UInt16GetDatum(unsignedInteger16)
	uint16	unsignedInteger16;
{
	Datum	datum;

	bzero(&datum, sizeof(Datum));
	DatumSet(datum, unsignedInteger16, unsignedInteger16);
	return (datum);
}

int32
DatumGetInt32(datum)
	Datum	datum;
{
	return (DatumGet(datum, integer32));
}

Datum
Int32GetDatum(integer32)
	int32	integer32;
{
	Datum	datum;

	bzero(&datum, sizeof(Datum));
	DatumSet(datum, integer32, integer32);
	return (datum);
}

uint32
DatumGetUInt32(datum)
	Datum	datum;
{
	return (DatumGet(datum, unsignedInteger32));
}

Datum
UInt32GetDatum(unsignedInteger32)
	uint32	unsignedInteger32;
{
	Datum	datum;

	bzero(&datum, sizeof(Datum));
	DatumSet(datum, unsignedInteger32, unsignedInteger32);
	return (datum);
}

float32
DatumGetFloat32(datum)
	Datum	datum;
{
	return (DatumGet(datum, smallFloat));
}

Datum
Float32GetDatum(smallFloat)
	float32	smallFloat;
{
	Datum	datum;

	bzero(&datum, sizeof(Datum));
	DatumSet(datum, smallFloat, smallFloat);
	return (datum);
}

float64
DatumGetFloat64(datum)
	Datum	datum;
{
	return (DatumGet(datum, largeFloat));
}

Datum
Float64GetDatum(largeFloat)
	float64	largeFloat;
{
	Datum	datum;

	bzero(&datum, sizeof(Datum));
	DatumSet(datum, largeFloat, largeFloat);
	return (datum);
}

Pointer
DatumGetPointer(datum)
	Datum	datum;
{
	return (DatumGet(datum, pointer));
}

Datum
PointerGetDatum(pointer)
	Pointer	pointer;
{
	Datum	datum;

	bzero(&datum, sizeof(Datum));
	DatumSet(datum, pointer, pointer);
	return (datum);
}

Pointer *
DatumGetPointerPointer(datum)
	Datum	datum;
{
	return (DatumGet(datum, pointerPointer));
}

Datum
PointerPointerGetDatum(pointerPointerInP)
	Pointer	*pointerPointerInP;
{
	Datum	datum;

	bzero(&datum, sizeof(Datum));
	DatumSet(datum, pointerPointer, pointerPointerInP);
	return (datum);
}

AnyStruct *
DatumGetStructPointer(datum)
	Datum	datum;
{
	return (DatumGet(datum, structPointer));
}

Datum
StructPointerGetDatum(structPointerInP)
	AnyStruct	*structPointerInP;
{
	Datum	datum;

	bzero(&datum, sizeof(Datum));
	DatumSet(datum, structPointer, structPointerInP);
	return (datum);
}

Name
DatumGetName(datum)
	Datum	datum;
{
	return (DatumGet(datum, name));
}

Datum
NameGetDatum(name)
	Name	name;
{
	Datum	datum;

	bzero(&datum, sizeof(Datum));
	DatumSet(datum, name, name);
	return (datum);
}

ObjectId
DatumGetObjectId(datum)
	Datum	datum;
{
	return (DatumGet(datum, objectId));
}

Datum
ObjectIdGetDatum(objectId)
	ObjectId	objectId;
{
	Datum	datum;

	bzero(&datum, sizeof(Datum));
	DatumSet(datum, objectId, objectId);
	return (datum);
}
