%{
static char *scan_l =
	"$Header$";
/**********************************************************************
  scan.l
  lexical scanner for POSTGRES 
 **********************************************************************/

#include "atoms.h"
#include "parse.h"
#include "pg_lisp.h"
#include "ctype.h"
#include "math.h"

#undef input
#undef unput

extern LispValue yylval;
%}

digit		[0-9]
letter		[_A-Za-z]
letter_or_digit	[_A-Za-z0-9]

identifier	{letter}{letter_or_digit}*

self		[,()\[\].;$\:]
operator	[\!\@\#\%\^\&\*\-\=\+\|\`\|\<\>\/\?]+

string		\"
character	"'"

integer		[-+]?{digit}+
real		[-+]?{digit}+\.{digit}+([Ee][-+]?{digit}+)?

comment		"/*"

space		[ \t\n\f]
other		.

%%
{comment}	{ scancmnt();		}
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
			yylval = lispFloat((double) atof(yytext));
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

