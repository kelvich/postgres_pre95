#include <stdio.h>
#include "executor.h"

/*
 * $Header$
 *
 * Executes the "not_in" operator for any data type
 *
 * XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
 * X HACK WARNING!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! X
 * XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
 *
 * This code is the OLD not-in code that is HACKED
 * into place until operators that can have arguments as
 * columns are ******REALLY****** implemented!!!!!!!!!!!
 *
 *
 */

bool
int4notin(not_in_arg, relation_and_attr)

int16 not_in_arg;
char *relation_and_attr;

{
    HeapTuple tuple;
    ObjectId scan_rel_oid;
    Relation relation_to_scan;
    int left_side_argument, integer_value;
    HeapTuple current_tuple;
    HeapScanDesc scan_descriptor;
    bool dummy, retval;
    int attrid;
    char *relation, *attribute;
    char my_copy[32];
	Datum value;

    strcpy(my_copy, relation_and_attr);

    relation = strtok(my_copy, ".");
    attribute = strtok(NULL, ".");


    /* fetch tuple OID */

    left_side_argument = not_in_arg;

    /* Open the relation and get a relation descriptor */

    relation_to_scan = amopenr(relation);
    attrid  = my_varattno(relation_to_scan, attribute);
    scan_descriptor = ambeginscan(relation_to_scan, false, NULL, NULL, 1);
    retval = true;

    /* do a scan of the relation, and do the check */
    for (current_tuple = amgetnext(scan_descriptor, 0, NULL);
         current_tuple != NULL && retval;
         current_tuple = amgetnext(scan_descriptor, 0, NULL))
    {
        value = HeapTupleGetAttributeValue(current_tuple,
                                           InvalidBuffer,
                                           (AttributeNumber) attrid,
										   &(relation_to_scan->rd_att),
                                           &dummy);

		integer_value = DatumGetInt16(value);
        if (left_side_argument == integer_value)
        {
            retval = false;
        }
    }

    /* close the relation */
    amclose(relation_to_scan);
    return(retval);
}

bool
oidnotin(oid, compare)
ObjectId oid;
char *compare;
{
	return(int4notin(oid, compare));
}

/*
 * XXX
 * If varattno (in parser/catalog_utils.h) ever is added to
 * cinterface.a, this routine should go away
 */

int
my_varattno ( rd , a)
     Relation rd;
     char *a;
{
	int i;
	
	for (i = 0; i < rd->rd_rel->relnatts; i++) {
		if (!strcmp(&rd->rd_att.data[i]->attname, a)) {
			return(i+1);
		}
	}
	return(-1);
}
