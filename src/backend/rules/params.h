/*
 * params.h
 * Declarations/definitions of stuff needed to handle parameterized plans.
 *
 * Identification:
 *     $Header$
 */

#ifndef ParamsIncluded
#define ParamsIncluded	1	/* Include this file only once */

#include "c.h"
#include "name.h"
#include "oid.h"
#include "datum.h"

/* ----------------------------------------------------------------
 *    ParamListInfo
 *
 *    Information needed in order for the executor to handle
 *    parameterized plans (you know,  $.salary, $.name etc. stuff...).
 *
 *    ParamListInfoData contains information needed when substituting a
 *    Param node with a Const node.
 *
 *      name   : the parameter name
 *      type   : PG_TYPE OID of the value
 *      length : length in bytes of the value
 *      isnull : true if & only if the value is null (if true then
 *               the fields 'length' and 'value' are undefined).
 *      value  : the value that has to be substituted in the place
 *               of the parameter.
 *
 *   ParamListInfo is to be used as an array of ParamListInfoData
 *   records. An 'InvalidName' in the name field of such a record
 *   indicates that this is the last record in the array.
 *
 * ----------------------------------------------------------------
 */

typedef struct ParamListInfoData {
    Name	name;
    ObjectId	type;
    Size	length;
    bool	isnull;
    Datum	value;
} ParamListInfoData;

typedef ParamListInfoData *ParamListInfo;

#endif
