/* 
 * atoms.c
 * - string,atom lookup thingy, reduces strcmp traffic greatly
 * in the bowels of the system
 *
 */

/*
 * List of (keyword-name, keyword-token-value) pairs.
 *
 * NOTE: This list must be sorted, because binary
 *	 search is used to locate entries.
 */

#include <ctype.h>
#include "tmp/c.h"

RcsId("$Header$");

#include "nodes/pg_lisp.h"
#include "parser/atoms.h"
#include "parser/parse.h"
#include "utils/log.h"

ScanKeyword	ScanKeywords[] = {
	/* name			value		*/
	{ "NULL",		PNULL		},
	{ "abort",		ABORT_TRANS	},
	{ "addattr",		ADD_ATTR	},
	{ "after",		AFTER		},
	{ "aggregate",          AGGREGATE       },
	{ "all",		ALL		},
	{ "always",		ALWAYS		},
	{ "and",		AND		},
	{ "append",		APPEND		},
	{ "arch_store",		ARCH_STORE	},
	{ "archive",		ARCHIVE		},
	{ "arg",		ARG		},
        { "as",                 AS              },
	{ "ascending",		ASCENDING	},
	{ "attachas",		ATTACH_AS	},
	{ "backward",		BACKWARD	},
	{ "before",		BEFORE		},
	{ "begin",		BEGIN_TRANS	},
	{ "binary",		BINARY		},
	{ "by",			BY		},
	{ "cfunction",		C_FUNCTION	},
	{ "close",		CLOSE		},
	{ "cluster",		CLUSTER		},
	{ "copy",		COPY		},
	{ "create",		CREATE		},
	{ "createdb",		CREATEDB	},
	{ "current",		CURRENT		},
	{ "define",		DEFINE		},
	{ "delete",		DELETE		},
	{ "demand",		DEMAND		},
	{ "descending",		DESCENDING	},
	{ "destroy",		DESTROY		},
	{ "destroydb",		DESTROYDB	},
	{ "do",			DO		},
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
	{ "instance",		P_TUPLE		},
	{ "instead",		INSTEAD		},
	{ "intersect",		INTERSECT	},
	{ "into",		INTO		},
	{ "intotemp",           INTOTEMP        },
	{ "iportal",            IPORTAL         },
	{ "is",			IS		},
	{ "key",		KEY		},
	{ "leftouter",		LEFTOUTER	},
	{ "light",		LIGHT		},
	{ "listen",             LISTEN          },
	{ "load",               LOAD            },
	{ "merge",		MERGE		},
	{ "move",		MOVE		},
	{ "never",		NEVER		},
	{ "new",		NEW		},
	{ "none",		NONE		},
	{ "nonulls",		NONULLS		},
	{ "not",		NOT		},
	{ "notify",             NOTIFY          },
	{ "on",			ON		},
	{ "once",		ONCE		},
	{ "operator",		OPERATOR	},
	{ "or",			OR		},
	{ "output_proc",	OUTPUTPROC	},
	{ "parallel",		PARALLEL	},
	{ "pfunction",		P_FUNCTION	},
	{ "portal",		PORTAL		},
	{ "postquel",		POSTQUEL	},
	{ "priority",		PRIORITY	},
	{ "purge",		PURGE		},
	{ "quel",		QUEL		},
	{ "relation",		RELATION	},
	{ "remove",		REMOVE		},
	{ "rename",		RENAME		},
	{ "replace",		REPLACE		},
	{ "retrieve",		RETRIEVE	},
	{ "returns",		RETURNS		},
	{ "rewrite",		REWRITE		},
	{ "rightouter",		RIGHTOUTER	},
	{ "rule",		RULE		},
	{ "sort",		SORT		},
	{ "stdin",		STDIN		},
	{ "stdout",		STDOUT		},
	{ "store",		STORE		},
	{ "to",			TO		},
	{ "transaction",	TRANSACTION	},
	{ "type",		P_TYPE		},
	{ "union",		UNION		},
	{ "unique",		UNIQUE		},
	{ "using",		USING		},
	{ "vacuum",		VACUUM		},
	{ "version",		NEWVERSION	},
	{ "view",		VIEW		},
	{ "where",		WHERE		},
	{ "with",		WITH		},
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

String
AtomValueGetString ( atomval )
     int atomval;
{
    ScanKeyword *low = &ScanKeywords[0];
    ScanKeyword *high = endof(ScanKeywords) - 1;
    int keyword_list_length = (high-low);
    int i;

    for (i=0; i < keyword_list_length  ; i++ )
      if (ScanKeywords[i].value == atomval )
	return(ScanKeywords[i].name);
    
    elog(WARN,"AtomGetString called with bogus atom # : %d", atomval );
    return(NULL);
}
