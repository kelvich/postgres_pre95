/*
 * params.h
 * Declarations/definitions of stuff needed to handle parameterized plans.
 *
 * Identification:
 *     $Header$
 */

#ifndef ParamsIncluded
#define ParamsIncluded	1	/* Include this file only once */

#include "tmp/postgres.h"
#include "tmp/name.h"
#include "tmp/datum.h"

/* ----------------------------------------------------------------
 *
 * The following are the possible values for the 'paramkind'
 * field of a Param node.
 *    
 * PARAM_NAMED: The parameter has a name, i.e. something
 *              like `$.salary' or `$.foobar'.
 *              In this case field `paramname' must be a valid Name.
 *              and field `paramid' must be == 0.
 *
 * PARAM_NUM:   The parameter has only a numeric identifier,
 *              i.e. something like `$1', `$2' etc.
 *              The number is contained in the `parmid' field.
 *
 * PARAM_NEW:   Used in PRS2 rule, similar to PARAM_NAMED.
 *              The `paramname' refers to the "NEW" tuple
 *              
 * PARAM_OLD:   Same as PARAM_NEW, but in this case we refer to
 *		the "OLD" tuple.
 */

#define PARAM_NAMED	11
#define PARAM_NUM	12
#define PARAM_NEW	13
#define PARAM_OLD	14
#define PARAM_INVALID   100


/* ----------------------------------------------------------------
 *    ParamListInfo
 *
 *    Information needed in order for the executor to handle
 *    parameterized plans (you know,  $.salary, $.name etc. stuff...).
 *
 *    ParamListInfoData contains information needed when substituting a
 *    Param node with a Const node.
 *
 *	kind   : the kind of parameter.
 *      name   : the parameter name (valid if kind == PARAM_NAMED,
 *               PARAM_NEW or PARAM_OLD)
 *      id     : the parameter id (valid if kind == PARAM_NUM)
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
    int		kind;
    Name	name;
    int32	id;
    ObjectId	type;
    Size	length;
    bool	isnull;
    Datum	value;
} ParamListInfoData;

typedef ParamListInfoData *ParamListInfo;

#endif
