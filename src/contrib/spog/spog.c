/*
 * spog (simple postgres query interface)
 *
 * $Id$
 *
 * $Log$
 * Revision 1.1  1993/02/18 23:29:29  aoki
 * Initial revision
 *
 * Revision 2.3  1992/08/13  11:44:48  schoenw
 * options -x and -n added, postgres v4r0 support
 *
 * Revision 2.2  1992/05/27  14:50:54  schoenw
 * HAVE_XMALLOC define for some readline versions added
 *
 * Revision 2.1  1992/05/22  12:53:33  schoenw
 * this is the public release 1.0
 *
 * Revision 1.6  1992/05/22  12:46:00  schoenw
 * builtin-commands, help functions and completion added
 *
 * Revision 1.5  1992/04/13  09:29:59  schoenw
 * histfilename hardcoded
 *
 * Revision 1.4  1992/03/11  14:54:50  schoenw
 * filename '-' for stdin
 *
 * Revision 1.3  1992/03/09  15:38:11  schoenw
 * -f option added
 *
 * Revision 1.2  1992/01/30  14:18:21  schoenw
 * old option -q removed
 *
 * Revision 1.1  1992/01/24  16:46:18  schoenw
 * Initial revision
 *
 */

#include "spog.h"

bool verbose = false;  /* be verbose -- return the no. of tuples fetched */
bool silent = false;   /* silence -- return only the status of the query */
bool print = false;    /* print -- print commands before execution */
bool norc = false;     /* norc -- dont read the initialization file */
char *progname;        /* the name of the game */
char *histfilename;    /* name of the current history file */
char *database;        /* name of the current database */

/* 
 * These symbols are referenced  by the new readline library.
 * So here they are.
 */

#ifdef NEED_XMALLOC

#ifdef ultrix
extern char *malloc(), *realloc();
#endif

char *xmalloc (n)
int n;
{
        return malloc (n);
}

char *xrealloc (s, n)
char *s;
int n;
{
        return realloc (s, n);
}
#endif

#ifdef NEED_STRDUP
/*
 * SunOS has strdup, other UNIXes (like Ultrix) don't.
 */
char *strdup(s)
char *s;
{
	char *news;

	if (!s)
		return((char *) NULL);
	if (!(news = malloc(strlen(s)) + 1))
		return((char *) NULL);
	return(strcpy(news, s));
}
#endif

/*
 * Strip whitespace from the start and end of a string.
 */

stripwhite (string)
char *string;
{
	register int i = 0;
	
	if (!string) return;
	
	while (whitespace (string[i]))
		i++;
	
	if (i)
		strcpy (string, string + i);
	
	i = strlen (string) - 1;
	
	while (i > 0 && whitespace (string[i]))
		i--;
	
	string[++i] = '\0';
}

/*
 * open and close a history file
 */

char *
open_history()
{
	char *fn;

	if (! (fn = malloc(255))) {
                fprintf (stderr, "%s: malloc failed", progname);
                exit (1);
        }

        strcpy (fn, getenv("HOME"));
        strcat (fn, "/.");
        strcat (fn, progname);
        strcat (fn, "_");
        strcat (fn, database);

	(void) read_history (fn);

	return fn;
}

void
close_history()
{
	stifle_history(HISTSIZE);
	
	if (write_history(histfilename))
		fprintf (stderr, "%s: error writing %s\n", 
			 progname, histfilename);
}

/*
 * get the commands from a file and process them
 */

query_from_file(file)
FILE *file;
{

	char c;
	char QUERY[MAXQUERYSIZE];
	int i = 0;
	int skip = false;

	while ((c = getc(file)) != EOF) {
		switch (c) {
		case '\n':
			QUERY[i]='\0';
			if (strlen(QUERY) > 0) {
				do_it(QUERY);
			}
			skip = false;
			i = 0;
			break;
		case '#':
			skip = true;
			break;
		default:
			if (skip) break;
			if (i == 0 && isspace(c)) break;
			QUERY[i++] = c;
			if (i == MAXQUERYSIZE)
				printf (stderr,"%s: query too long\n", progname);
		}
	}
}

/*
 * get the commands using readline and process them
 */

query_from_readline()
{
	char *line = NULL;
	char prompt[80];

	sprintf(prompt, "%s > ", progname);
	printf("connected to backend ");
	if (PQhost) printf("on %s ", PQhost);
	if (PQport) printf("port %s ", PQport);
	printf("database %s\n", database);

	initialize_readline();
	
	using_history ();

	histfilename = open_history();
	
	while (1)
	{
		if (line != (char *)NULL)
			free (line);
		
		line = readline (prompt);

		stripwhite (line);

		if (line && *line) {
			do_it(line);
			add_history (line);
		}

		if (!line) {
			printf ("\n");
			break;
		}

		if (!strcmp(line,"exit"))
			break;

		if (!strcmp(line,"quit"))
			break;
	}

	close_history();
}

/*
 * execute commands in $HOME/.spogrc
 */

read_spogrc()
{
	char *home, *filename;
	FILE *file;

	if ((home = getenv("HOME")) != NULL) {
		if (filename = malloc(strlen(home)+10)) {
			strcpy(filename,home);
			strcat(filename,"/.spogrc");

			if ((file = fopen(filename,"r")) == NULL) {
				return;
			}
			
			if (verbose) printf ("reading %s\n", filename);

			query_from_file (file);
			fclose (file);
		}
	}
}

/*
 * execute commands in a given file filename
 */

read_file(filename)
char *filename;
{
	FILE *file;

	if (!strcmp(filename,"-")) {
		file = stdin;
	} else {
		if ((file = fopen(filename,"r")) == NULL) {
			fprintf(stderr, "%s: can't open %s\n", 
				progname, filename);
			return;
		}
	}

	query_from_file (file);
	
        if (file != stdin) fclose(file);
}

/*
 * here we start
 */

main(argc,argv)
int argc;
char **argv;
{
	char *filename = NULL;
	char *query = NULL;

	int c;
	int errflg = 0;

	if ((progname = rindex(argv[0], '/')) == NULL)
		progname = argv[0];
	else
		progname++;

	while ((c = getopt(argc,argv,"h:p:c:f:vsxn?")) != EOF) {
		switch (c) {
		case 'h':
			PQhost = optarg;
			break;
		case 'p':
			PQport = optarg;

			break;
		case 'c':
			query = optarg;
			break;
		case 'f':
			filename = optarg;
			break;
		case 'v':
			verbose = true;
			break;
		case 's':
			silent = true;
			break;
		case 'x':
			print = true;
			break;
		case 'n':
			norc = true;
			break;
		case '?':
			errflg++;
		}
	}

	if (errflg) {
		fprintf(stderr, "usage: %s [-h host] [-p port] [-c query] [-f file] [-s] [-v] [-x] [-n] database\n", progname);
		exit (2);
	}

	/* find default database */
	if ((database = argv[optind]) == NULL)
		if ((database = getenv("PGDATABASE")) == NULL) {
			if ((database = getenv("USER")) == NULL) {
				database = "template1";
			};
			printf ("Warning: Assuming database %s\n",database);
		}
	
	PQsetdb(database);

	/* first process the $HOME/.spogrc */
	if (!norc) read_spogrc();

	/* process a single query */
	if (query) {
		do_it(query);
		PQfinish();
		exit (0);
	}

	/* process queries from a file */
	if (filename) {
		read_file(filename);
		PQfinish();
		exit (0);
	}

	/* ok, we are interactive -- that's where the fun beginns */
	query_from_readline();
	PQfinish();
	exit (0);

}
