/*
 * lispread.c
 *
 * Herein lies a function to read strings printed by lispDisplay.
 * Additionally, there is a lisp token parser, and a lisp 
 * 
 * the functions are lispRead, lsptok, and LispToken.
 *
 * Bugs:  (nil) won't work.
 */

#include <ctype.h>


#include "palloc.h"
#include "pg_lisp.h"
#include "log.h"
#include "atoms.h"

/*
 * 	Global declaration for LispNil.
 * 	Relies on the definition of "nil" in the LISP image.
 *	XXX Do *NOT* move this below the inclusion of c.h, or this
 *	    will break when used with Franz LISP!
 */

extern bool _equalLispValue();
extern void lispDisplayFp();
extern void lispDisplay();

#include "c.h"
#include "nodes.h"

#define lispAlloc() (LispValue)palloc(sizeof(struct _LispValue))

#define RIGHT_PAREN (1000000 + 1)
#define LEFT_PAREN  (1000000 + 2)
#define PLAN_SYM    (1000000 + 3)

/*
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

	if (isdigit(*token))
	{
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

LispValue lispRead(read_car_only)

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
					this_value->cdr = lispRead(false);
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
			fprintf(stderr, "bad type\n");
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
		if (length == NULL) return(NULL);
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
		(*length++); local_str++;
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

/*
 * This function fronts the more generic lispRead function by calling
 * lsptok with length == NULL, which just passes lsptok the string.
 */

LispValue lispReadString(string)

char *string;

{
	(void) lsptok(string, NULL);
	return(lispRead(true));
}
