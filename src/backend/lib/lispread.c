/*
 * lispread.c
 *
 * Herein lies a function to read strings printed by lispDisplay.
 * Additionally, there is a lisp token parser, and a lisp 
 * 
 * the functions are lispRead, lsptok, and LispToken.
 *
 * Bugs:  (nil) won't work.
 *
 * Identification:
 *    $Header$
 */

#include <stdio.h>
#include <ctype.h>

#include "tmp/postgres.h"

RcsId("$Header$");

#include "parser/atoms.h"
#include "rules/params.h"
#include "utils/palloc.h"
#include "utils/log.h"

#include "nodes/nodes.h"
#include "nodes/pg_lisp.h"
#include "nodes/primnodes.h"
#include "nodes/primnodes.a.h"

extern bool _equalLispValue();
extern void lispDisplayFp();
extern void lispDisplay();
extern int16 get_typlen();
extern bool get_typbyval();
double atof();

#define RIGHT_PAREN (1000000 + 1)
#define LEFT_PAREN  (1000000 + 2)
#define PLAN_SYM    (1000000 + 3)

/*----------------------------------------------------------
 *
 * Param Node handling.
 *
 * makeParamList is a global variable. If it is set to 'true' then
 * every time a Param node is generated, we add an entry to
 * paramList (another global variable).
 * maxParamListEntries is the current maximum number of
 * entries the array can handle.
 */

static bool makeParamList;
static ParamListInfo paramList;
static int maxParamListEntries;

static
void
addToParamList();

/*-------------------------------------------------------------
 * LispToken returns the type of the lisp token contained in token.
 * It returns one of
 * 
 * PGLISP_ATOM
 * PGLISP_DTPR
 * PGLISP_FLOAT
 * PGLISP_INT
 * PGLISP_STR
 * PGLISP_VECI
 *
 * and some of its own:
 *
 * RIGHT_PAREN
 * LEFT_PAREN
 * PLAN_SYM
 * DOTPLAN_SYM
 * 
 * LispToken will check only the
 * first character for many lisp objects, such as strings or vectors.
 *
 * Does no error checking as it assumes the file is electronically
 * generated.
 */

NodeTag lispTokenType(token, length)

char *token;
int length;

{
	NodeTag retval;
	int sign;

	/*
	 * Check if the token is a number (decimal or integer,
	 * positive or negative
	 */
	if (isdigit(*token) ||
	   (length>=2 && *token=='-' && isdigit(*(token+1)) ))
	{
		/*
		 * skip the optional '-' (i.e. negative number)
		 */
		if (*token == '-') {
		    token++;
		}

		/*
		 * See if there is a decimal point
		 */

		for (; length && *token != '.'; token++, length--);
		
		/*
		 * if there isn't, token's an int, otherwise it's a float.
		 */

		retval = (*token != '.') ? PGLISP_INT : PGLISP_FLOAT;
	}
	else if (isalpha(*token))
		retval = PGLISP_ATOM;
	else if (*token == '(')
		retval = LEFT_PAREN;
	else if (*token == ')')
		retval = RIGHT_PAREN;
	else if (*token == '\"')
		retval = PGLISP_STR;
	else if (*token == '.' && length == 1)
		retval = PGLISP_DTPR;
	else if (*token == '#' && token[1] == 'S' && length == 2)
		retval = PLAN_SYM;
	else if (*token == '#' 
              && token[1] == '<'
              && isdigit(token[2])
	      && length < 2)
		retval = PGLISP_VECI;
	return(retval);
}

/*
 * This guy does all the reading.
 *
 * Secrets:  He assumes that lsptok already has the string (see below).
 * Any callers should set read_car_only to true.
 */

LispValue
lispRead(read_car_only)

bool read_car_only;

{
	char *token, *lsptok();
	NodeTag type, lispTokenType();
	LispValue this_value, return_value, parsePlanString();
	int tok_len;
	int i, nbytes;
	char tmp;
	bool make_dotted_pair_cell = false;

	token = lsptok(NULL, &tok_len);

	if (token == NULL) return(NULL);

	type = lispTokenType(token, tok_len);

	
	switch(type)
	{
		case PLAN_SYM:
			token = lsptok(NULL, &tok_len);
			if (token[0] != '(') return(NULL);
			this_value = parsePlanString(NULL);
			token = lsptok(NULL, &tok_len);
			if (token[0] != ')') return(NULL);
			if (makeParamList) {
			    if (IsA(this_value,Param)) {
				addToParamList(this_value);
			    }
			}
			if (!read_car_only)
				make_dotted_pair_cell = true;
			else
				make_dotted_pair_cell = false;
			break;
		case LEFT_PAREN:
			if (!read_car_only)
			{
				this_value = lispDottedPair();
				this_value->val.car = lispRead(false);
				this_value->cdr = lispRead(false);
				if (this_value->cdr == (LispValue) -1)
				{
					this_value->cdr = CAR(lispRead(false));
				}
			}
			else
			{
				this_value = lispRead(false);
			}
			break;
		case RIGHT_PAREN:
			this_value = LispNil;
			break;
		case PGLISP_ATOM:
			if (!strncmp(token, "nil", 3))
			{
				this_value = LispNil;
				/*
				 * It might be "nil" but it is an atom!
				 */
				if (read_car_only) {
				    make_dotted_pair_cell = false;
				} else {
				    make_dotted_pair_cell = true;
				}
			}
			else
			{

				tmp = token[tok_len];
				token[tok_len] = '\0';
				this_value = lispAtom(token);
				token[tok_len] = tmp;
				make_dotted_pair_cell = true;
			}
			break;
		case PGLISP_FLOAT:
			tmp = token[tok_len];
			token[tok_len] = '\0';
			this_value = lispFloat(atof(token));
			token[tok_len] = tmp;
			make_dotted_pair_cell = true;
			break;
		case PGLISP_INT:
			tmp = token[tok_len];
			token[tok_len] = '\0';
			this_value = lispInteger(atoi(token));
			token[tok_len] = tmp;
			make_dotted_pair_cell = true;
			break;
		case PGLISP_STR:
			tmp = token[tok_len - 1];
			token[tok_len - 1] = '\0';
			token++;
			this_value = lispString(token);
			token[tok_len - 2] = tmp;
			make_dotted_pair_cell = true;
			break;
		case PGLISP_VECI:
			sscanf(token, "#<%d:", &nbytes);
			this_value = lispVectori(nbytes);
			for (i = 0; i < nbytes; i++)
			{
				token = lsptok(NULL, tok_len);
				this_value->val.veci->data[i] = atoi(token);
			}
			token = lsptok(NULL, tok_len); /* get > */
			make_dotted_pair_cell = true;
			break;
		case PGLISP_DTPR:
			this_value = (LispValue) -1;
			break;
		default:
			elog(WARN, "Bad type (lispRead)");
			break;
	}
	if (make_dotted_pair_cell)
	{
		return_value = lispDottedPair();
		return_value->val.car = this_value;
		if (!read_car_only)
		{
			return_value->cdr = lispRead(false);
			if (return_value->cdr == (LispValue) -1)
			{
				return_value->cdr = lispRead(false);
			}
		}
		else
		{
			return_value->cdr = LispNil;
		}
	}
	else
	{
		return_value = this_value;
	}
	return(return_value);
} /* lispRead */

/*
 * Works kinda like strtok, except it doesn't put nulls into string.
 * 
 * Returns the length in length instead.  The string can be set without
 * returning a token by calling lsptok with length == NULL.
 *
 */

char *lsptok(string, length)

char *string;
int  *length;

{
	static char *local_str;
	char *ret_string;
	

	if (string != NULL) 
	{
		local_str = string;
		if (length == NULL) {
		    return(NULL);
		}
	}


	for (; *local_str == ' '
	    || *local_str == '\n'
	    || *local_str == '\t'; local_str++);

	/*
	 * Now pointing at next token.
	 */

	ret_string = local_str;
	if (*local_str == '\0') return(NULL);
	*length = 1;

	if (*local_str == '\"')
	{
		for (local_str++; *local_str != '\"'; (*length)++, local_str++);
		(*length)++; local_str++;
	}
	else if (*local_str == ')' || *local_str == '(')
	{
		local_str++;
	}
	else
	{
		for (; *local_str != ' '
		    && *local_str != '\n'
		    && *local_str != '\t'
		    && *local_str != '('
		    && *local_str != ')'; local_str++, (*length)++);
		(*length)--;
	}
	return(ret_string);
}

/*---------------------------------------------------------------------
 * This function fronts the more generic lispRead function by calling
 * lsptok with length == NULL, which just passes lsptok the string.
 *
 * There are two version of lispReadString.
 *
 *      lispReadString (the simple one)
 *           and
 *      lispReadStringWithParams
 *
 * the second one takes en extra (output) argument, 'thisParamListP'
 * If it is non NULL then when the routine returns it will point to
 * a 'ParamLispInfo' which contains information about all the Param
 * nodes of the LispValue returned.
 *
 */

LispValue
lispReadStringWithParams(string, thisParamListP)

char *string;
ParamListInfo *thisParamListP;

{
	LispValue res;

	if (thisParamListP == NULL) {
	    makeParamList = false;	/* global variable */
	}
	else {
	    makeParamList = true;	/* global variable */
	    paramList = NULL;		/* global variable */
	}

	(void) lsptok(string, NULL);
	res = lispRead(true);

	if (thisParamListP != NULL) {
	    *thisParamListP = paramList;
	}
	return(res);
}
			    
LispValue
lispReadString(string)
char *string;
{
    LispValue res;

    res = lispReadStringWithParams(string, NULL);
    return(res);
}

/*-----------------------------------------------------------
 *
 * addToParamList
 *
 * Add an entry for the given Param node to array
 * 'paramList' (thsi is a global variable).
 *
 * 'paramList' points to a place in memory where 'ParamListInfoData'
 * structs are contiguously stored. A struct with its field 'kind'
 * equal to PARAM_INVALID is always the last entry.
 */

static
void
addToParamList(paramNode)
Param paramNode;
{
    int size;
    int oldSize;
    int i;
    int delta = 10;
    ParamListInfo temp;

    if (paramList == NULL) {
	/*
	 * this is the first entry.
	 * allocate some space for a few entries...
	 */
	maxParamListEntries = delta;
	size = sizeof(ParamListInfoData) * maxParamListEntries;
	paramList = (ParamListInfo) palloc(size);
	Assert(PointerIsValid(paramList));
	paramList[0].kind = PARAM_INVALID;
	elog(DEBUG,"added initial entry");
    }

    /*
     * search the array until either another entry with
     * the same name  and paramkind is found
     * or till we reach the end of the array.
     */
    i = 0;
    while(paramList[i].kind != PARAM_INVALID) {
	int match = false;
	if (paramList[i].kind != get_paramkind(paramNode)) continue;
	switch (paramList[i].kind) {
	case PARAM_NUM:
	    match = (paramList[i].id == get_paramid(paramNode));
	    break;
	case PARAM_NAMED: 	
	case PARAM_NEW:	
	case PARAM_OLD:
	default: elog(WARN, "unexpected kind of parameter node");
		break;
	}
	if (match) break;
	i+=1;
    }
/*	    if ((PARAM_NUM != get_paramkind(paramNode)) &&
		!NameIsEqual(paramList[i].name, get_paramname(paramNode))
		&& paramList[i].kind == get_paramkind(paramNode))
		*/


    if (paramList[i].kind != PARAM_INVALID) {
	/*
	 * we have found a duplicate entry
	 */
	return;
    }

    /*
     * No entry exists for this parameter, therefore we must add
     * a  new entry at the end of the array.
     * We must first check to see if the array is big enough
     * to hold this new entry. If not, then we have to reallocate
     * some space.
     */

    if ((i+1) >= maxParamListEntries) {
	oldSize = sizeof(ParamListInfoData) * maxParamListEntries;
	maxParamListEntries += delta;
	size = sizeof(ParamListInfoData) * maxParamListEntries;
	temp = (ParamListInfo) palloc(size);
	Assert(PointerIsValid(temp));
	/*
	 * copy the entries of the old array to the new one
	 */
	bcopy((char *)paramList, (char *)temp, (unsigned int) oldSize);
	pfree(paramList);
	paramList = temp;
    }

    /*
     * Ok, there is enough room for a new entry.
     */
    paramList[i].kind = get_paramkind(paramNode);
    paramList[i].name = get_paramname(paramNode); /* Note: no copy! */
    paramList[i].id = get_paramid(paramNode);
    paramList[i].type = get_paramtype(paramNode);
    paramList[i].length = (Size) get_typlen(get_paramtype(paramNode));
    paramList[i].byval = get_typbyval(get_paramtype(paramNode));

    /*
     * mark the next entry in paramList as invalid...
     */
    paramList[i+1].kind = PARAM_INVALID;

}


