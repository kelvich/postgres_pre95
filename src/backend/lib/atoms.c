/* 
 * atoms.c
 * - string,atom lookup thingy, reduces strcmp traffic greatly
 * in the bowels of the system
 *
 * $Header$
 */

/*
 * List of (keyword-name, keyword-token-value) pairs.
 *
 * NOTE: This list must be sorted, because binary
 *	 search is used to locate entries.
 */

#include "pg_lisp.h"
#include "atoms.h"
#include "ctype.h"
#include "parse.h"

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


