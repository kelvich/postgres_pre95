/*
 * command interpreter part of spog
 *
 * $Id$
 *
 * $Log$
 * Revision 1.1  1993/02/18 23:27:32  aoki
 * Initial revision
 *
 * Revision 2.2  1992/08/13  11:44:48  schoenw
 * options -x and -n added, postgres v4r0 support
 *
 * Revision 2.1  1992/05/22  12:53:33  schoenw
 * this is the public release 1.0
 *
 * Revision 1.1  1992/05/22  12:46:00  schoenw
 * Initial revision
 *
 */

#include "spog.h"
#include <sys/wait.h>

extern int query_continues;

/* 
 * The names of functions that actually do the manipulation.
 * (They are all defined below except do_query() )
 */

int com_help (), com_exit (), com_status(), com_verbose(), com_silent();
int com_database(), com_host(), com_port(), com_shell();
int com_format (), com_separator();
int do_query ();

/* 
 * list of all commands accepted by mr. spog
 */

COMMAND commands[] = {
{ "?", com_help, 
	  "a synonym for `help'", 
	  "? [command]" },
{ "!", com_shell, 
	  "a synonym for `shell'", 
	  "! command" },
{ "exit", com_exit, 
	  "exit spog", 
	  NULL },
{ "quit", com_exit, 
	  "exit spog", 
	  NULL },
{ "status", com_status, 
	  "display the current settings", 
	  NULL },
{ "help", com_help, 
	  "give some help", 
	  "help [command]" },
{ "verbose", com_verbose, 
	  "set verbose mode on or off", 
	  "verbose [on | off]" },
{ "silent", com_silent,
	  "set silent mode on or off", 
	  "silent [on | off]" },
{ "database", com_database,
	  "set name of database",
	  "database dbname" },
{ "host", com_host,
	  "set hostname of backend",
	  "host hostname" },
{ "port", com_port,
	  "set port number of backend",
	  "port number" },
{ "shell", com_shell,
	  "execute a shell command",
	  "shell command" },
{ "format", com_format, 
	  "set the output format of an attribute or a type", 
	  "format ( attribute name | type name | global ) \"formatstring\"" },
{ "separator", com_separator, 
	  "set the tuple separator", 
	  "separator \"formatstring\"" },

{ "abort", do_query, 
	  "abort the current transaction",
	  "abort" },
{ "addattr", do_query, 
	  "add an attribute to an existing relation",
	  "addattr ( attname1 = type1 {, attnamei = typei } ) to classname" },
{ "append", do_query, 
	  "append tuples to a relation",
	  "append classname ( att_name1 = expression1 {, att_namei = expressioni } ) \n\t[ from from_list ] [ where qual ]" },
/* { "attachas", do_query,
	  "reestablish communication using an existing port"
	  "attaches name" }, */
{ "begin", do_query, 
	  "begin a transaction",
	  "begin" },
{ "close", do_query, 
	  "close a portal",
	  "close [ portal_name ]" },
/* { "cluster", do_query,
	  "give storage clustering advice to POSTGRES",
	  "cluster classname on attname [ using operator ]" }, */
{ "copy", do_query, 
	  "copy data to or from a class from or to a file",
	  "copy [ binary ] classname direction (\"filename\" | stdin | stdout)" },
{ "create", do_query, 
	  "create a new class",
	  "create classname ( attname = type {, attname = type } ) \n\t[ inherits (classname {, classname } ) ] \n\t[ archive = archive_mode ]\n\t[ store = \"smgr_name\" ]\n\t[ arch_store = \"smgr_name\" ]" },
{ "createdb", do_query, 
	  "create a new database",
	  "createdb dbname" },
{ "create version", do_query,
	  "create a new version",
	  "create version classname1 from classname2" },
{ "define aggregate", do_query, 
	  "define a new aggregate",
	  "define aggregate agg_name [as]\n\t(sfunc1 = state_transition_function1,\n\tsfunc2 = state_transition_function2,\n\tfinalfunc = final_function,\n\tinitcond1=initial_condition1,\n\tinitcond2 = initial_condition2)" },
{ "define function", do_query, 
	  "define a new function",
	  "define function function_name ( language = {\"c\"|\"postquel\"}, returntype = type\n\t[, percall_cpu = \"costly{!*}\"]\n\t[, perbyte_cpu = \"costly{!*}\"]\n\t[, outin_ratio = percentage]\n\t[, byte_pct = percentage]\n\t) arg is ( type1 [, typen ] ) as {\"filename\",\"postquel queries\"} " },
{ "define index", do_query,
	  "construct a secondary index",
	  "define index index_name on relation using am_name (att_name type_ops)" },
{ "define operator", do_query,
	  "define a new user operator",
	  "define operator operator_name ( arg1 = type1 [, arg2 = type2 ] \n\t , procedure = func_name \n\t [, precedence = number] \n\t [, associativity = (left | right | none | any)] \n\t [, commutator = com_op ] \n\t [, negator = neg_op ] \n\t [, restrict = res_proc ] \n\t [, hashes] \n\t [, join = join_proc ] \n\t [, sort = sort_op1 {, sort_op2}] )" },
{ "define rule", do_query,
	  "define a new rule",
	  "define [ instance | rewrite ] rule rule_name is \n\t on event to object [ [ from clause ] where clause ] \n\t do [ instead ] [ action | nothing | \'[\' action ... \']\' ]" },
{ "define type", do_query,
	  "define a new base data type",
	  "define type typename ( internallength = ( number | variable ), \n\t [ externallength = (number | variable),] \n\t input = function, output = function ) \n\t [, element = typename] \n\t [, delimiter = <character>] \n\t [, default = \"string\"] \n\t [, send = procedure] \n\t [, receive = procedure] \n\t [, passedbyvalue] )" },
{ "define view", do_query,
	  "construct a virtual class",
	  "define view view_name ( [ dom_name_1 = ] expression_1 {, [ dom_name_i = ] expression_i } ) \n\t [ from from_list ] [ where qual ]" },
{ "delete", do_query,
	  "delete instances from a class",
	  "delete instance_variable [ from from_list ] [ where qual ]" },
{ "destroy", do_query,
	  "destroy existing classes",
	  "destroy classname1 {, classnamei }" },
{ "destroydb", do_query,
	  "destroy an existing database",
	  "destroydb dbname" },
{ "end", do_query,
	  "commit the current transaction",
	  "end" },
{ "fetch", do_query,
	  "fetch instance(s) from a portal",
	  "fetch [(forward | backward)] [(number | all)] [ in portal_name ]" },
{ "listen", do_query,
	  "listen for notification on a relation",
	  "listen relation"},
{ "load", do_query,
	  "dynamically load an object file",
	  "load \"filename\"" },
/* { "merge", do_query,
	  "merge two classes",
	  "merge classname1 into classname2" }, */
/* { "move", do_query,
	  "move the pointer in a portal",
	  "move [ forward | backward ] [ number | all | to ( number | record_qual ) ] [ in portal_name ]" }, */
{ "notify", do_query,
	  "signal all frontends and backends listening on relation",
	  "notify relation"},
{ "purge", do_query,
	  "discard historical data",
	  "purge classname [ before abstime ] [ after reltime ]" },
{ "remove aggregate", do_query,
	  "remove the definition of an aggregate",
	  "remove aggregate agg_name" },
{ "remove function", do_query,
	  "remove a user defined function",
	  "remove function functionname" },
{ "remove index", do_query,
	  "remove an index",
	  "remove index index_name" },
{ "remove operator", do_query,
	  "remove an operator",
	  "remove operator opr_desc" },
{ "remove rule", do_query,
	  "remove a rule",
	  "remove rule [ instance | rewrite ] rule rule_name" },
{ "remove type", do_query,
	  "remove an user-defined type from the system catalogs",
	  "remove type typename" },
{ "rename", do_query,
	  "rename a class or an attribute in a class",
	  "rename classname1 to classname2\nrename attname1 in classname to attname2" },
{ "replace", do_query,
	  "replace values of attributes in a class",
	  "replace instance_variable ( att_name1 = expression1 {, att_namei = expressioni } ) \n\t [ from from_list ] [ where qual ]" },
{ "retrieve", do_query,
	  "retrieve instances from a class",
	  "retrieve \n\t [ ( into classname ) | ( portal portal_name ) | iportal portal_name ] \n\t [ unique ] \n\t ( [ attname1 = ] expression1 {, [ attnamei = ] expressioni } ) \n\t [ from from_list ] \n\t [ where qual ] \n\t [ sort by attname-1 [ using operator ] {, attname-j [ using operator ]} " },
{ "vacuum", do_query,
	  "vacuum a database",
	  "vacuum" },
{ "fast path", do_query, 
	  "trap door into system internals",
	  "retrieve ( retval = function ( [ arg {, arg } ] ) )" },
{ (char *)NULL, (Function *)NULL, 
	  (char *)NULL, 
	  (char *)NULL }
};

/* list of postgres token that are not valid commands */

TOKEN token[] = {
	"abort", "addattr", "append", "attachas", "begin",
	"close", "cluster", "copy", "create", "define",
	"delete", "destroy", "end", "fetch", "merge",
	"purge", "remove", "rename", "replace", "retrieve",
	"all", "always", "after", "and", "archive",
	"arch_store", "arg", "ascending", "backward", "before",
	"binary", "by", "demand", "descending", "empty", 
	"forward", "from", "heavy", "intersect", "into",
	"in", "index", "inherits", "input", "is", 
        "key", "language", "leftouter", "light", "listen", "store",
	"merge", "never", "none", "not", "notify", "on", 
        "or", "output", "portal", "priority", "returntype",
        "rightouter", "rule", "sort", "to", "transaction",
	"union", "unique", "using", "where", "with", 
	"function", "operator", "inheritance", "version", "current", 
        "new", "then", "do", "instead", "view", 
        "rewrite", "postquel", "relation", "returns", "intotemp", 
        "load", "createdb", "destroydb", "stdin", "stdout", 
        "vacuum", "aggregate",
	(char *) NULL
};

/*
 * some utilities
 */

COMMAND *
find_command (name)
char *name;
{
	register int i, n;
	char *space;
	
	for (i = 0; commands[i].name; i++) {
		n = strlen(commands[i].name);
		if ((space = index(commands[i].name,' ')) != NULL) {
			n = space - commands[i].name;
		}
		if (strncmp (name, commands[i].name, n) == 0)
			return (&commands[i]);
	}
	
	return ((COMMAND *)NULL);
}

char *
skip_word(str)
char *str;
{
        /* Get argument to command, if any. */
        while (*str && !whitespace (*str))
                str++;
        while (*str && whitespace (*str))
                str++;

	return str;
}

void
first_word(str, word)
char *str, *word;
{
	char *p, *q, *line;
	
	if ((q = line = strdup(str)) == NULL) {
		printf ("%s\n", "cant malloc");
		*word = '\0';
		return;
	}

	p = word;
	while ( *q && whitespace (*q) ) q++;
	while ( *q && !whitespace (*q) ) {
		*p = *q;
		q++;
		p++;
	}
	*p = 0;

	free (line);
}

char *
parse_fmt(str)
char *str;
{
	char *p, *q, *fmt;
	int special=0;

	if ((p = fmt = strdup(str)) == NULL) {
		printf ("%s\n", "cant malloc");
                return NULL;
	}

	q = str;
	while ( *q && whitespace (*q) ) q++;
	if ( *q != '"' ) {
		printf ("format must beginn with a `\"'\n");
		return NULL;
	}
	q++;

	while ( *q && (*q != '"') ) {
		if ( *q != '\\' ) {
			if (special && ( *q == 'n' || *q == 't')) {
				*p = *q == 'n' ? '\n' : '\t';
			} else {
				*p = *q;
			}
			p++;
			special = 0;
                } else {
			if (special) {
				*p = '\\';
				p++;
				special = 0;
			} else {
				special = 1;
			}
		}
		q++;
	}

	if ( *q && (*q == '"') ) {
		*p = 0;
		return fmt;
	} else {
		printf ("format does not end with a `\"'\n");
		return NULL;
	}
}

/* 
 * and here we start
 */

void
do_it(line)
char *line;
{
	register int i;
	COMMAND *command;
	char *word;

	if (print) printf("%s\n",line);

	if (query_continues) {
		do_query (line);
		return;
	}
	
	if ((word = strdup(line)) == NULL) {
		printf ("%s\n", "cant malloc");
		return;
	}
	
	first_word(line, word);
	
	if (*word == '\0') {
		free (word);
		return;
	}
	
	command = find_command (word);
	
	if (!command)
	{
		fprintf (stderr, "Sorry, %s can't %s.\n", progname, word );
		free (word);
		return;
	}
	
	(*(command->func)) (line);

	free (word);
	
}

/*
 * here are all the built-in commands implemented
 */

com_help(arg)
char *arg;
{
	register int i;
	int printed = 0;

	arg = skip_word (arg);

	if (!*arg) {
		printf ("help `command' gives you a hint of how to use `command'.");
		printf (" Possibilties are:\n");
		for (i = 0; commands[i].name; i++) {
			printf ("%s", commands[i].name);
			if (strlen(commands[i].name) < 16)
				printf ("%s", strlen(commands[i].name) < 8
					? "\t\t" : "\t");
			printf (" -- %s\n", commands[i].doc);
		}
		return;
	}

	/* Call the function. */
	for (i = 0; commands[i].name; i++)
	{
		if (!*arg || (strcmp (arg, commands[i].name) == 0))
		{
			if (commands[i].syntax) {
				printf ("%s\t -- %s:\n\n%s\n", 
					commands[i].name,
					commands[i].doc,
					commands[i].syntax);
			} else {
				printf ("%s\t -- %s\n", 
					commands[i].name, commands[i].doc);
			}
			printed++;
		}
	}
	
	if (!printed)
	{
		printf ("No commands match `%s'.  Possibilties are:\n", arg);
		
		for (i = 0; commands[i].name; i++)
		{
			if (strlen(commands[i].name) < 16) {
				if (strlen(commands[i].name) < 8 )
					printf ("%s\t\t\t", commands[i].name);
				else
					printf ("%s\t\t", commands[i].name);
			} else 
				printf ("%s\t",commands[i].name);
			printed++;
			if ((printed % 3) == 0) printf ("\n");
		}		
		printf ("\n");
	}
}

com_exit(arg)
char *arg;
{
	/* just do nothing */
}

com_status(arg)
char *arg;
{
        printf ("connected to backend ");
        if (PQhost) printf ("on %s ", PQhost);
        if (PQport) printf ("port %s ", PQport);
        printf ("database %s", database);
	
	if (!verbose && !silent) {
		printf ("\n");
		return;
	}
	if (verbose && silent) {
		printf ("  (verbose,silent)\n");
		return;
	}
	if (verbose && !silent) {
		printf ("  (verbose)\n");
		return;
	}
	if (!verbose && silent) {
		printf ("  (silent)\n");
		return;
	}
	printf ("separator:\t%s\n", separator);
}

com_verbose(arg)
char *arg;
{
	register int i;
	
	arg = skip_word (arg);
	
	if (!strcmp(arg,"on")) {
		verbose = 1;
	} else if (!strcmp(arg,"off")) {
		verbose = 0;
	} else {
		printf ("use `verbose on' or `verbose off'\n");
	}
}
com_silent(arg)
char *arg;
{
	register int i;

	arg = skip_word (arg);
	
	if (!strcmp(arg,"on")) {
		silent = 1;
	} else if (!strcmp(arg,"off")) {
		silent = 0;
	} else {
		printf ("use `silent on' or `silent off'\n");
	}
}

com_database(arg)
char *arg;
{
	arg = skip_word (arg);
	
	if (!*arg) {
		printf ("use `database dbname'\n");
		return;
	}

	PQfinish();
	PQsetdb(database=strdup(arg));

	close_history();
	histfilename = open_history();
}

com_host(arg)
char *arg;
{
	arg = skip_word (arg);
	
	if (!*arg) {
		printf ("use `host hostname'\n");
		return;
	}

	PQsetdb(arg);
	PQhost = strdup(arg);
}

com_port(arg)
char *arg;
{
	arg = skip_word (arg);
	
	if (!*arg) {
		printf ("use `port number'\n");
		return;
	}

	PQfinish();
	PQport = strdup(arg);
}

com_shell(arg)
char *arg;
{
	char *shell="/bin/sh";
	char *env;
	int pid;
	int statusp;

	arg = skip_word (arg);

	system(arg);
}

struct FMT_NODE *
insert_node (list, node)
struct FMT_NODE *list, *node;
{
	bool found = 0;
	struct FMT_NODE *ptr;

	node -> next = NULL;
  
	if (list == NULL) {
		list = node;
	} else {
		ptr = list;
		while ((ptr != NULL) && (! found)) {
			if (strcmp (ptr -> name, node -> name) == 0) {
				free (ptr -> fmt);
				ptr -> fmt = node -> fmt;

				free (node -> name);
				free ((char *) node);

				found = 1;
			} else {
				ptr = ptr -> next;
			}
		}

		if (! found) {
			node -> next = list;
			list = node;
		}
	}

	return (list);
}

com_format(arg)
char *arg;
{
        register int i,j;
	int type = 1;
        char *word;
	struct FMT_NODE *node, *insert_node();
	char QUERY[MAXQUERYSIZE];
	PortalBuffer *PortBuf, *PQparray();
	char *res;
	
	node = (struct FMT_NODE *) malloc (sizeof(struct FMT_NODE));
	if (node == NULL) {
		printf ("%s\n", "cant malloc");
		return;
	}

	if ((word = strdup(arg)) == NULL) {
		printf ("%s\n", "cant malloc");
		return;
	}
	
	arg = skip_word (arg);
	first_word(arg, word);
	if (*word == '\0') {
		free (word);
		return;
	}
	
	if (!strcmp(word,"type"))
		type = 1;
	else if (!strcmp(word,"attribute"))
		type = 0;
	else if (!strcmp(word,"global"))
		type = -1;
	else {
		printf ("use `format (type name | attribute name | global) formatstring'\n");
		free (word);
		return;
	}
	
	arg = skip_word (arg);
	first_word(arg, word);
	if (*word == '\0') {
		free (word);
		return;
	}

	if (type == -1) {
		if (parse_fmt(word) == NULL ) return;
		default_fmt = parse_fmt(word);
		return;
	}

	node -> name = word;

	arg = skip_word (arg);
	if ((word = parse_fmt(arg)) == NULL) {
		free (word);
		return;
	}

	node -> fmt = word;

	if (type) {
		PQexec("begin");
		sprintf (QUERY, "retrieve portal port (%s) where %s=\"%s\"",
			 "pg_type.oid", "pg_type.typname", node -> name);
		res = PQexec (QUERY);
		if (res[0] == 'R') {
			free ((char *) node);
			return;
		}
		PQexec("fetch all in port");
		PortBuf = PQparray("port");
		if (PQntuples(PortBuf) > 0) {
			node -> oid = atoi(PQgetvalue(PortBuf, 0, 0));

			type_fmt = insert_node (type_fmt, node);
		} else {
			free ((char *) node);
		}
		PQexec("close port");		
		PQexec("end");
	} else {
                att_fmt = insert_node (att_fmt, node);
	}
}

com_separator(arg)
char *arg;
{
        register int i;
	char *fmt;

	arg = skip_word(arg);

	if ((fmt = parse_fmt(arg)) != NULL)
		separator = fmt;
}


/* Generator function for command completion.  STATE lets us know whether
   to start from scratch; without any state (i.e. STATE == 0), then we
   start at the top of the list. */

char *
command_generator (text, state)
char *text;
int state;
{
	static int list_index, len;
	char *name;
	
	/* If this is a new word to complete, initialize now.  This 
	   includes saving the length of TEXT for efficiency, and 
	   initializing the index variable to 0. */
	
	if (!state) {
		list_index = 0;
		len = strlen (text);
	}
	
	/* Return the next name which partially matches from 
	   the command list. */
	 
	while (name = commands[list_index].name) {
		list_index++;
		
		if (strncasecmp (name, text, len) == 0)
			return (name);
	}
	
	/* If no names matched, then return NULL. */
	return ((char *)NULL);
}

char *
token_generator (text, state)
char *text;
int state;
{
	static int list_index, len;
	char *name;
	
	/* If this is a new word to complete, initialize now.  This 
	   includes saving the length of TEXT for efficiency, and 
	   initializing the index variable to 0. */
	
	if (!state) {
		list_index = 0;
		len = strlen (text);
	}
	
	/* Return the next name which partially matches from 
	   the command list. */
	 
	while (name = token[list_index].name) {
		list_index++;

		if (strncasecmp (name, text, len) == 0)
			return (name);
	}
	
	/* If no names matched, then return NULL. */
	return ((char *)NULL);
}

/* Attempt to complete on the contents of TEXT.  START and END show the
   region of TEXT that contains the word to complete.  We can use the
   entire line in case we want to do some simple parsing.  Return the
   array of matches, or NULL if there aren't any. */

char **
spog_completion (text, start, end)
char *text;
int start, end;
{
	char **matches;
	char *command_generator ();
	matches = (char **)NULL;
	
	/* Try to complete the command or the token */
	if (start == 0) {
		matches = completion_matches (text, command_generator);
	} else {
		matches = completion_matches (text, token_generator);
	}
	
	return (matches);
}

initialize_readline ()
{
	char **spog_completion ();
	extern int rl_completion_query_items;
	
	rl_completion_query_items = 137; /* don't ask me */

	/* Tell the completer that we want a crack first. */
	rl_attempted_completion_function = (Function *)spog_completion;
}
