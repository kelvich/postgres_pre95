%{
static char *scan_l =
	"$Header$";
/**********************************************************************
  scan.l
  lexical scanner for POSTGRES 
 **********************************************************************/

#include "parse.h"
/*#include "catalog_utils.h"*/
/*#include "global.h"*/
#include "lispdep.h"
#include "ctype.h"

#undef input
#undef unput
/*
#define lengthof(byte_array)	(sizeof(byte_array) / sizeof((byte_array)[0]))
#define endof(byte_array)	(&byte_array[lengthof(byte_array)])
*/
typedef struct ScanKeyword {
	char	*name;
	int	value;
} ScanKeyword;

ScanKeyword	*ScanKeywordLookup();

extern LispValue yylval;
%}

digit		[0-9]
letter		[_A-Za-z]
letter_or_digit	[_A-Za-z0-9]

identifier	{letter}{letter_or_digit}*

self		[,()\[\].;$]
operator	[\!\@\#\%\^\&\*\-\=\+\|\`\|\:\<\>\/\?]+

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
				yylval = lispInteger(keyword->value);
				return (keyword->value);
			} else {
				yylval = lispString(yytext);
				return (IDENT);
			}
		}
{space}		{ /* void */		}
{other}		{ return (yytext[0]);	}

%%

/*
 * List of (keyword-name, keyword-token-value) pairs.
 *
 * NOTE: This list must be sorted, because binary
 *	 search is used to locate entries.
 */
ScanKeyword	ScanKeywords[] = {
	/* name			value		*/
	{ "abort",		ABORT_TRANS	},
	{ "addattr",		ADD_ATTR	},
	{ "after",		AFTER		},
	{ "all",		ALL		},
	{ "always",		ALWAYS		},
	{ "and",		AND		},
	{ "append",		APPEND		},
	{ "archive",		ARCHIVE		},
	{ "arg",		ARG		},
	{ "ascending",		ASCENDING	},
	{ "attachas",		ATTACH_AS	},
	{ "backward",		BACKWARD	},
	{ "before",		BEFORE		},
	{ "begin",		BEGIN_TRANS	},
	{ "binary",		BINARY		},
	{ "by",			BY		},
	{ "close",		CLOSE		},
	{ "cluster",		CLUSTER		},
	{ "copy",		COPY		},
	{ "create",		CREATE		},
	{ "define",		DEFINE		},
	{ "delete",		DELETE		},
	{ "demand",		DEMAND		},
	{ "descending",		DESCENDING	},
	{ "destroy",		DESTROY		},
	{ "empty",		EMPTY		},
	{ "end",		END_TRANS	},
	{ "execute",		EXECUTE		},
	{ "fetch",		FETCH		},
	{ "forward",		FORWARD		},
	{ "from",		FROM		},
	{ "function",		FUNCTION	},
	{ "heavy",		HEAVY		},
	{ "in",			IN		},
	{ "index",		INDEX		},
	{ "indexable",		INDEXABLE	},
	{ "inherits",		INHERITS	},
	{ "input_proc",		INPUTPROC	},
	{ "intersect",		INTERSECT	},
	{ "into",		INTO		},
	{ "is",			IS		},
	{ "key",		KEY		},
	{ "leftouter",		LEFTOUTER	},
	{ "light",		LIGHT		},
	{ "merge",		MERGE		},
	{ "move",		MOVE		},
	{ "never",		NEVER		},
	{ "none",		NONE		},
	{ "nonulls",		NONULLS		},
	{ "not",		NOT		},
	{ "on",			ON		},
	{ "once",		ONCE		},
	{ "operator",		OPERATOR	},
	{ "or",			OR		},
	{ "output_proc",	OUTPUTPROC	},
	{ "portal",		PORTAL		},
	{ "priority",		PRIORITY	},
	{ "purge",		PURGE		},
	{ "quel",		QUEL		},
	{ "remove",		REMOVE		},
	{ "rename",		RENAME		},
	{ "replace",		REPLACE		},
	{ "retrieve",		RETRIEVE	},
	{ "rightouter",		RIGHTOUTER	},
	{ "rule",		RULE		},
	{ "sort",		SORT		},
	{ "to",			TO		},
	{ "type",		P_TYPE		},
	{ "transaction",	TRANSACTION	},
	{ "union",		UNION		},
	{ "unique",		UNIQUE		},
	{ "using",		USING		},
	{ "version",		NEWVERSION	},
	{ "where",		WHERE		},
	{ "with",		WITH		},
	{ "NULL",		PNULL		},
};

ScanKeyword *
ScanKeywordLookup(text)
	char	*text;
{
	ScanKeyword	*low	= &ScanKeywords[0];
	ScanKeyword	*high	= endof(ScanKeywords) - 1;
	ScanKeyword	*middle;
	int		difference;

	while (low <= high) {
		middle = low + (high - low) / 2;
		difference = strcmp(middle->name, text);
		if (difference == 0)
			return (middle);
		else if (difference < 0)
			low = middle + 1;
		else
			high = middle - 1;
	}

	return (NULL);
}

/**************************************************
  MakeUpper 
  - destructively changes the inputstr, converting
  any lower-case characters to their corresponding
  upper case equivalents
 **************************************************/

MakeUpper(mixedstr)
	char *mixedstr;
{
	while(*mixedstr != NULL )  {
		if(islower(*mixedstr)) 
			*mixedstr = toupper(*mixedstr);
		mixedstr++;
	}
}

MakeLower(mixedstr)
	char *mixedstr;
{
	while(*mixedstr != NULL )  {
		if(isupper(*mixedstr)) 
			*mixedstr = tolower(*mixedstr);
		mixedstr++;
	}
}




