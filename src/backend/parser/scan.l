%{
static char *scan_l =
	"$Header$";
/**********************************************************************
  scan.l
  lexical scanner for POSTGRES 
 **********************************************************************/

#include <ctype.h>
#include <math.h>

#include "parser/parse.h"
#include "nodes/pg_lisp.h"
#include "parser/atoms.h"


#undef input
#undef unput

extern LispValue yylval;
%}

digit		[0-9]
letter		[_A-Za-z]
letter_or_digit	[_A-Za-z0-9]

identifier	{letter}{letter_or_digit}*

self		[,()\[\].;$\:\+\-\*\/\<\>\=]
op_and_self	[\~\!\@\#\%\^\&\|\`\|\?\$\:\+\-\*\/\<\>\=]
op_only		[\~\!\@\#\%\^\&\|\`\|\?]

operator	({op_and_self}{op_and_self}+)|{op_only}+
string		\"
specialstr	\`
character	"'"

integer		[-+]?{digit}+
real		[-+]?{digit}+\.{digit}+([Ee][-+]?{digit}+)?

comment		"/*"

space		[ \t\n\f]
other		.

%%
{comment}	{ scancmnt();		}
"::"		{ return TYPECAST;	}
{specialstr}	{
			char buf[8192];
			scanspecial(buf,sizeof(buf));
			yylval = lispString(buf);
			return (SCONST);
		}
{self}		{ return (yytext[0]);	}
{operator}	{
			yylval = lispString(yytext);
			return (Op);
		}
{integer}	{
			yylval = lispInteger(atoi(yytext));
			return (ICONST);
		}
{real}		{
			yylval = lispFloat(atof(yytext));
			return (FCONST);
		}
{string}	{
			char buf[8192];
			scanstr(buf,sizeof(buf));
			yylval = lispString(buf);
			return (SCONST);
		}

{character}	{
			char buf[2];
			scanchar(buf);
			yylval = lispString(buf);
			return (CCONST);
		}
{identifier}	{
			ScanKeyword	*keyword;

			keyword = ScanKeywordLookup(yytext);
			if (keyword != NULL) {
				yylval = lispAtom(keyword->name);
				return (keyword->value);
			} else {
				yylval = lispString(yytext);
				return (IDENT);
			}
		}
{space}		{ /* void */		}
{other}		{ return (yytext[0]);	}

%%

/**************************************************
  MakeUpper 
  - destructively changes the inputstr, converting
  any lower-case characters to their corresponding
  upper case equivalents
 **************************************************/
/*
	XXX - unused code, remove sometime in the future

MakeUpper(mixedstr)
	char *mixedstr;
{
	while(*mixedstr != NULL )  {
		if(islower(*mixedstr)) 
			*mixedstr = toupper(*mixedstr);
		mixedstr++;
	}
}
*/

/*
	XXX - unused code, remove sometime in the future

MakeLower(mixedstr)
	char *mixedstr;
{
	while(*mixedstr != NULL )  {
		if(isupper(*mixedstr)) 
			*mixedstr = tolower(*mixedstr);
		mixedstr++;
	}
}
*/

