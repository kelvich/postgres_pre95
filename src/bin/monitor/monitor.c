/*    
 *     POSTGRES Terminal Monitor
 *    
 */

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <strings.h>
#include <sys/file.h>
#include <sys/param.h>	/* for MAXHOSTNAMELEN */
#include <sys/types.h>
#include <sys/uio.h>

#include "tmp/c.h"
#include "libpq/auth.h"
#include "tmp/libpq-fe.h"
#include "tmp/pqcomm.h"
#include "utils/log.h"

RcsId("$Header$");


/* Global Declarations 
 *
 *  These variables are defined in pqexec.c and
 *  are used in the fe/be comm. protocol
 *   
 */
extern char	*getenv();
extern char	*PQhost;     /* machine on which the backend is running */
extern char	*PQport;     /* comm. port with the postgres backend. */
extern char	*PQtty;      /* the tty where postgres msgs are displayed */
extern char	*PQdatabase; /* the postgres db to access.  */
extern int	PQportset;   /* 1 if comm. with backend is set */
extern int	PQxactid;    /* xact id of the current xact.  */
extern char	*PQinitstr;  /* initialisation string sent to backend */
extern int	PQtracep;    /* set to 1 if debugging is set */


/* 
 * monitor.c  -- function prototypes (all private)
 */
int do_input ARGS((FILE *ifp ));
int init_tmon ARGS((void ));
int welcome ARGS((void ));
int handle_editor ARGS((void ));
int handle_shell ARGS((void ));
int print_tuples ARGS((char *pname ));
int handle_send ARGS((void ));
int handle_execution ARGS((char *query ));
int handle_one_command ARGS((char *command ));
int handle_file_insert ARGS((FILE *ifp ));
int handle_print ARGS((void ));
int handle_exit ARGS((int exit_status ));
int handle_clear ARGS((void ));
int handle_print_time ARGS((void ));
int handle_write_to_file ARGS((void ));
int handle_help ARGS((void ));
int stuff_buffer ARGS((char c ));
void argsetup ARGS((int *argcP, char ***argvP));
void handle_copy_in ARGS((void));
void handle_copy_out ARGS((void));

/*      
 *          Functions which maintain the logical query buffer in
 *          /tmp/PQxxxxx.  It in general just does a copy from input
 *          to query buffer, unless it gets a backslash escape character.
 *          It recognizes the following escapes:
 *      
 *          \e -- enter editor
 *          \g -- "GO": submit query to POSTGRES
 *          \i -- include (switch input to external file)
 *          \p -- print query buffer 
 *          \q -- quit POSTGRES
 *          \r -- force reset (clear) of query buffer
 *          \s -- call shell
 *          \t -- print current time
 *          \w -- write query buffer to external file
 *          \h -- print the list of commands
 *          \? -- print the list of commands
 *          \\ -- produce a single backslash in query buffer
 *      
 */

/*
 *   Declaration of global variables (but only to the file monitor.c
 */

#define DEFAULT_EDITOR "/usr/ucb/vi"
#define COPYBUFSIZ	8192
static char *user_editor;     /* user's desired editor  */
static int tmon_temp;         /* file descriptor for temp. buffer file */
static char *pid_string;
static char *tmon_temp_filename;
static char query_buffer[8192];  /* Max postgres buffer size */
bool RunOneCommand = false;
bool Debugging;
bool Verbose;
bool Silent;
bool TerseOutput = false;
bool PrintAttNames = true;

extern char *optarg;
extern int optind,opterr;
extern FILE *debug_port;

/*
 *  As of release 4, we allow the user to specify options in the environment
 *  variable PGOPTION.  These are treated as command-line options to the
 *  terminal monitor, and are parsed before the actual command-line args.
 *  The arge struct is used to construct an argv we can pass to getopt()
 *  containing the union of the environment and command line arguments.
 */

typedef struct arge {
    char	*a_arg;
    struct arge	*a_next;
} arge;

main(argc,argv)
     int argc;
     char **argv;
{
    int c;
    int errflag = 0;
    char *progname;
    char *debug_file;
    char *dbname;
    char *command;
    int exit_status = 0;
    char hostbuf[MAXHOSTNAMELEN];
    char *username, usernamebuf[USER_NAMESIZE+1];

    /* 
     * Processing command line arguments.
     *
     * h : sets the hostname.
     * p : sets the coom. port
     * t : sets the tty.
     * o : sets the other options. (see doc/libpq)
     * d : enable debugging mode.
     * q : run in quiet mode
     * Q : run in VERY quiet mode (no output except on errors)
     * c : monitor will run one POSTQUEL command and exit
     *
     * T : terse mode - no formatting
     * N : no attribute names - only columns of data
     *     (these two options are useful in conjunction with the "-c" option
     *      in scripts.)
     */

    progname = *argv;
    Debugging = false;
    Verbose = true;
    Silent = false;

    /* prepend PGOPTION, if any */
    argsetup(&argc, &argv);

    while ((c = getopt(argc, argv, "a:h:p:t:d:qTNQc:")) != EOF) {
	switch (c) {
	    case 'a':
	      fe_setauthsvc(optarg);
	      break;
	    case 'h' :
	      PQhost = optarg;
	      break;
	    case 'p' :
	      PQport = optarg;
	      break;
	    case 't' :
	      PQtty = optarg;
	      break;
	    case 'T' :
	      TerseOutput = true;
	      break;
	    case 'N' :
	      PrintAttNames = false;
	      break;
	    case 'd' :

	      /*
	       *  When debugging is turned on, the debugging messages
	       *  will be sent to the specified debug file, which
	       *  can be a tty ..
	       */

	      Debugging = true;
	      debug_file = optarg;
	      debug_port = fopen(debug_file,"w+");
	      if (debug_port == NULL) {
		  fprintf(stderr,"Unable to open debug file %s \n", debug_file);
		  exit(1);
	      }
	      PQtracep = 1;
	      break;
	    case 'q' :
	      Verbose = false;
	      break;
	    case 'Q' :
	      Verbose = false;
	      Silent = true;
	      break;
	    case 'c' :
	      Verbose = false;
	      Silent = true;
	      RunOneCommand = true;
	      command = optarg;
	      break;
	    case '?' :
	    default :
	      errflag++;
	      break;
	}
    }

    if (errflag ) {
      fprintf(stderr, "usage: %s [options...] [dbname]\n", progname);
      fprintf(stderr, "\t-a authsvc\tset authentication service\n");
      fprintf(stderr, "\t-c command\t\texecute one command\n");
      fprintf(stderr, "\t-d debugfile\t\tdebugging output file\n");
      fprintf(stderr, "\t-h host\t\t\tserver host name\n");
      fprintf(stderr, "\t-p port\t\t\tserver port number\n");
      fprintf(stderr, "\t-q\t\t\tquiet output\n");
      fprintf(stderr, "\t-t logfile\t\terror-logging tty\n");
      fprintf(stderr, "\t-N\t\t\toutput without attribute names\n");
      fprintf(stderr, "\t-Q\t\t\tREALLY quiet output\n");
      fprintf(stderr, "\t-T\t\t\tterse output\n");
      exit(2);
    }

    /* Determine our username (according to the authentication system, if
     * there is one).
     */
    if ((username = fe_getauthname()) == (char *) NULL) {
	    fprintf(stderr, "%s: could not find a valid user name\n",
		    progname);
	    exit(2);
    }
    bzero(usernamebuf, sizeof(usernamebuf));
    (void) strncpy(usernamebuf, username, USER_NAMESIZE);
    username = usernamebuf;
    
    /*
     *  Determine the hostname of the database server.  Try to avoid using
     * "localhost" if at all possible.
     */
    if (!PQhost && !(PQhost = getenv("PGHOST")))
	    PQhost = "localhost";
    if (!strcmp(PQhost, "localhost")) {
	    if (gethostname(hostbuf, MAXHOSTNAMELEN) != -1)
		    PQhost = hostbuf;
    }

    /* find database */
    if (!(dbname = argv[optind]) &&
	!(dbname = getenv("DATABASE")) &&
	!(dbname = username)) {
	    fprintf(stderr, "%s: no database name specified\n", progname);
	    exit (2);
    }

    PQsetdb(dbname);

    /* make sure things are ok before giving users a warm welcome! */
    check_conn_and_db();

    /* print out welcome message and start up */
    welcome();
    init_tmon(); 

    /* parse input */
    if (RunOneCommand) {
	exit_status = handle_one_command(command);
    } else {
	do_input(stdin);
    }

    handle_exit(exit_status);
}

do_input(ifp)
    FILE *ifp;
{
    int c;
    char escape;

    /*
     *  Processing user input.
     *  Basically we stuff the user input to a temp. file until
     *  an escape char. is detected, after which we switch
     *  to the appropriate routine to handle the escape.
     */

    if (ifp == stdin) {
	if (Verbose)
	    fprintf(stdout,"\nGo \n* ");
	else {
	    if (!Silent)
		fprintf(stdout, "* ");
	}
    }
    while ((c = getc(ifp)) != EOF ) {
	if ( c == '\\') {
	    /* handle escapes */
	    escape = getc(ifp);
	    switch( escape ) {
	      case 'e':
		handle_editor();
		break;
	      case 'g':
		handle_send();
		break;
	      case 'i':
		handle_file_insert(ifp);
		break;
	      case 'p':
		handle_print();
		break;
	      case 'q':
		handle_exit(0);
		break;
	      case 'r':
		handle_clear();
		break;
	      case 's':
		handle_shell();
		break;
	      case 't':
		handle_print_time();
		break;
	      case 'w':
		handle_write_to_file();
		break;
	      case '?':
	      case 'h':
		handle_help();
		break;
	      case '\\':
		c = escape;
		stuff_buffer(c); 
		break;
	      default:
		fprintf(stderr, "unknown escape given\n");
		break;
	    } /* end-of-switch */
	    if (ifp == stdin && escape != '\\') {
		if (Verbose)
		    fprintf(stdout,"\nGo \n* ");
		else {
		    if (!Silent)
			fprintf(stdout, "* ");
		}
	    }
	} else {
	    stuff_buffer(c); 
	}
    }
}
/*
 * init_tmon()
 *
 * set the following :
 *     user_editor, defaults to DEFAULT_EDITOR if env var is not set
 */

init_tmon()
{
    if (!RunOneCommand)
    {
	char *temp_editor = getenv("EDITOR");
    
	if (temp_editor != NULL) 
	    user_editor = temp_editor;
	else
	    user_editor = DEFAULT_EDITOR;

	tmon_temp_filename = malloc(20);
	sprintf(tmon_temp_filename, "/tmp/PQ%d", getpid());
	tmon_temp = open(tmon_temp_filename,O_CREAT | O_RDWR | O_APPEND,0666);
    }

    /*
     * Catch signals so we can delete the scratch file GK
     * but only if we aren't already ignoring them -mer
     */

    if (signal(SIGHUP, SIG_IGN) != SIG_IGN)
	signal(SIGHUP, handle_exit);
    if (signal(SIGQUIT, SIG_IGN) != SIG_IGN)
	signal(SIGQUIT, handle_exit);
    if (signal(SIGTERM, SIG_IGN) != SIG_IGN)
	signal(SIGTERM, handle_exit);
    if (signal(SIGINT, SIG_IGN) != SIG_IGN)
	signal(SIGINT, handle_exit);
}

/*
 *  check_conn_and_db checks the connection and the database
 */
check_conn_and_db()
{
    char *string= PQexec(" ");
    switch(*string) {
    case 'E':
    case 'R':
	handle_exit(1);
	break;
    }
}

/*
 *  welcome simply prints the Postgres welcome mesg.
 */

welcome()
{
    if (Verbose) {
	fprintf(stdout,"Welcome to the POSTGRES terminal monitor\n");
    }
}


/*
 *  handle_editor()
 *
 *  puts the user into edit mode using the editor specified
 *  by the variable "user_editor".
 */

handle_editor()
{
    char edit_line[100];

    close(tmon_temp);
    sprintf(edit_line,"%s %s",user_editor,tmon_temp_filename);
    system(edit_line);
    tmon_temp = open(tmon_temp_filename,O_CREAT | O_RDWR | O_APPEND,0666);
}

handle_shell()
{
    char *user_shell;

    user_shell = getenv("SHELL");
    if (user_shell != NULL) {
        system(user_shell);
    } else {
        system("/bin/sh");
    }
}
/*
 * print_tuples()
 *
 * This is the routine that prints out the tuples that
 * are returned from the backend.
 * Right now all columns are of fixed length,
 * this should be changed to allow wrap around for
 * tuples values that are wider.
 */

print_tuples ( pname)
     char *pname;
{
    PortalBuffer *p;
    int j,k,g,m,n,t,x;
    int temp, temp1;
    char *tborder;

    /* Now to examine all tuples fetched. */
    
    p = PQparray(pname);
    g = PQngroups (p);
    t = 0;
    temp = 0;

    for (k =0; k < g-1; k++) {
    temp += PQntuplesGroup(p,k);
    }

    /* Now to print out the attribute names 
     *
     * XXX Cheap Hack. For now, we see only the last group
     * of tuples.  This is clearly not the right
     * way to do things !!
     */

    n = PQntuplesGroup(p,g-1);  /* zero based index.  */
    m = PQnfieldsGroup(p,g-1);
    
    if ( m > 0 ) {  /* only print tuples with at least 1 field.  */

	if (!TerseOutput)
	{
		temp1 = (int) m * 14;
		tborder = malloc (temp1+1);
		bzero(tborder, temp1+1);
		for (x = 0; x <= temp1; x++) 
	    	tborder[x] = '-';

		tborder[x] = '\0';
		fprintf(stdout,"%s\n",tborder);
	}

	for (x=0; x < m; x++) 
	{
		if (PrintAttNames && !TerseOutput)
		{
	    	fprintf(stdout,"| %-12s",PQfnameGroup(p,g-1,x));
		}
		else if (PrintAttNames)
		{
	    	fprintf(stdout," %-12s",PQfnameGroup(p,g-1,x));
		}
	}

	if (PrintAttNames && !TerseOutput)
	{
		fprintf(stdout, "|\n");
		fprintf(stdout,"%s\n",tborder);
	}
	else if (PrintAttNames)
	{
		fprintf(stdout, "\n");
	}

	/* Print out the tuples */
	t += temp;
	
	for (x = 0; x < n; x++) {

	    for(j = 0; j < m; j++) 
		{
			if (!TerseOutput)
			{
				fprintf (stdout, "| %-12s", PQgetvalue(p,t+x,j));
			}
			else
			{
				fprintf (stdout, " %-12s", PQgetvalue(p,t+x,j));
			}
		}
				

		if (!TerseOutput)
		{
	    	fprintf (stdout, "|\n");
	    	fprintf(stdout,"%s\n",tborder);
		}
		else
		{
			fprintf(stdout, "\n");
		}
	}
    }
}


/*
 * handle_send()
 *
 * This is the routine that initialises the comm. with the
 * backend.  After the tuples have been returned and 
 * displayed, the query_buffer is cleared for the 
 * next query.
 *
 */

#include <ctype.h>
handle_send()
{
    char c  = (char)0;
    off_t pos;
    int cc = 0;
    int i = 0;
    bool InAComment = false;

    pos = lseek(tmon_temp,(off_t)0,L_SET);

    if (pos != 0)
	fprintf(stderr, "Bogus file position\n");

    if (Verbose)
	printf("\n");

    /* discard leading white space */
    while ( ( cc = read(tmon_temp,&c,1) ) != 0 && isspace((int)c))
	continue;

    if ( cc != 0 ) {
	pos = lseek(tmon_temp,(off_t)-1,L_INCR);
    }
    query_buffer[0] = 0;

    /*
     *  Stripping out comments (if any) from the query (should really be
     *  handled in the parser, of course).
     */
    while ( ( cc = read(tmon_temp,&c,1) ) != 0) {
	if (!InAComment) {
	    switch(c) {
	      case '\n':
		  query_buffer[i++] = ' ';
		  break;
	      case '/':
		{
		    int temp; 
		    char temp_c;
		    if ((temp = read(tmon_temp,&temp_c,1)) != 0) {
			if (temp_c == '*' )
			    InAComment = true;
			else {
			    query_buffer[i++] = c;
			    query_buffer[i++] = temp_c;
			}
		    }
		}
		break;
	      default:
		query_buffer[i++] = c;
		break;
	    }
	} else {

	    /*
	     *  in comment mode, drop chars until a '*' followed by a '/'
	     *  is found
	     */

	    int temp;
	    char temp_c;

	    if ( c == '*' ) {
		if ((temp = read(tmon_temp,&temp_c,1)) != 0) {
		    if ( temp_c == '/')
		      InAComment = false;
		} else {
		    fprintf(stderr, 
			    "\nWARN: input ended while inside a comment!\n");
		    handle_exit(1);
		}
	    }
	}
    }

    if (query_buffer[0] == 0) {
        query_buffer[0] = ' ';
        query_buffer[1] = 0;
    }

    if (Verbose)
	fprintf(stdout,"Query sent to backend is \"%s\"\n", query_buffer);
    fflush(stderr);
    fflush(stdout);
    
    /*
     * Repeat commands until done.
     */

	handle_execution(query_buffer);

    /* clear the query buffer and temp file -- this is very expensive */
    handle_clear();
    bzero(query_buffer,i);
}

/*
 * Actually execute the query in *query.
 *
 * Returns 0 if the query finished successfully, 1 otherwise.
 */

int
handle_execution(query)
    char *query;
{
    bool done = false;
    bool entering = true;
    char *pstring1;
    char pstring[256];
    int nqueries = 0;
    int retval = 0;
    
    while (!done) {
        if (entering) {
	    pstring1 = PQexec(query); 
	    fflush(stdout);
	    fflush(stderr);
	    entering = false;
	} 
	
	strcpy(pstring, pstring1);
	
        switch(pstring[0]) {
        case 'A':
        case 'P':
            print_tuples( &(pstring[1]));
            PQclear(&(pstring[1]));
            pstring1 = PQexec(" ");
            nqueries++;
            break;

        case 'C':
	    if (!Silent)
		fprintf(stdout,"%s",&(pstring[1]));
            if (!Verbose && !Silent)
		fprintf(stdout,"\n");
            pstring1 = PQexec(" ");
            nqueries++;
            break;
            
        case 'I':
            PQFlushI(nqueries - 1);
            done = true;
            break;
            
        case 'E':
        case 'R':
            done = true;
	    retval = 1;
            break;
	    
        case 'B':		/* copy to stdout */
	    handle_copy_out();
	    done = true;
	    break;
	case 'D':		/* copy from stdin */
	    handle_copy_in();
            done = true;
	    break;
        default:
            fprintf(stderr,"unknown type\n");
	}
	fflush(stderr);
	fflush(stdout);
    }
    return(retval);
}

/*
 * handle_one_command()
 *
 * allows a POSTQUEL command to be specified from the command line
 */

handle_one_command(command)
    char *command;
{
    return(handle_execution(command));
}

/*
 * handle_file_insert()
 *
 * allows the user to insert a query file and execute it.
 * NOTE: right now the full path name must be specified.
 */
 
handle_file_insert(ifp)
    FILE *ifp;
{
    char user_filename[50];
    FILE *nifp;

    fscanf(ifp, "%s",user_filename);
    nifp = fopen(user_filename, "r");
    if (nifp == (FILE *) NULL) {
        fprintf(stderr, "Cannot open %s\n", user_filename);
    } else {
        do_input(nifp);
        fclose (nifp);
    }
}

/*
 * handle_print()
 *
 * This routine prints out the contents (query) of the temp. file
 * onto stdout.
 */

handle_print()
{
    char c;
    off_t pos;
    int cc;
    
    pos = lseek(tmon_temp,(off_t)0,L_SET);
    
    if (pos != 0 )
	fprintf(stderr, "Bogus file position\n");
    
    printf("\n");
    
    while ( ( cc = read(tmon_temp,&c,1) ) != 0) 
	putchar(c);
    
    printf("\n");
}


/*
 * handle_exit()
 *
 * ends the comm. with the backend and exit the tm.
 */

handle_exit(exit_status)
    int exit_status;
{
    int unlink_status;
    
    if (!RunOneCommand) {
	close(tmon_temp);
	unlink(tmon_temp_filename);
    }
    PQfinish();   
    exit(exit_status);
}

/*
 * handle_clear()
 *
 *  This routine clears the temp. file.
 */

handle_clear()
{
    /* high cost */
    close(tmon_temp);
    tmon_temp = open(tmon_temp_filename,O_TRUNC|O_RDWR|O_CREAT ,0666);
}

/*
 * handle_print_time()
 * prints out the date using the "date" command.
 */

handle_print_time()
{
    system("date");
}

/*
 * handle_write_to_file()
 *
 * writes the contents of the temp. file to the
 * specified file.
 */

handle_write_to_file()
{
    char filename[50];
    static char command_line[512];
    int status;
    
    status = scanf("%s", filename);
    if (status < 1 || !filename[0]) {
	fprintf(stderr, "error: filename is empty\n");
	return(-1);
    }
    
    /* XXX portable way to check return status?  $%&! ultrix ... */
    (void) sprintf(command_line, "rm -f %s", filename);
    (void) system(command_line);
    (void) sprintf(command_line, "cp %s %s", tmon_temp_filename, filename);
    (void) system(command_line);

    return(0);
}

/*
 *
 * Prints out a help message.
 *
 */
 
handle_help()
{
    printf("Available commands include \n\n");
    printf("\\e -- enter editor\n");
    printf("\\g -- \"GO\": submit query to POSTGRES\n");
    printf("\\i -- include (switch input to external file)\n");
    printf("\\p -- print query buffer\n");
    printf("\\q -- quit POSTGRES\n");
    printf("\\r -- force reset (clear) of query buffer\n");
    printf("\\s -- shell escape \n");
    printf("\\t -- print current time\n");
    printf("\\w -- write query buffer to external file\n");
    printf("\\h -- print the list of commands\n");
    printf("\\? -- print the list of commands\n");
    printf("\\\\ -- produce a single backslash in query buffer\n");
    fflush(stdin);
}

/*
 * stuff_buffer()
 *
 * writes the user input into the temp. file.
 */

stuff_buffer(c)
    char c;
{
    int cc;

    cc = write(tmon_temp,&c,1);

    if(cc == -1)
	fprintf(stderr, "error writing to temp file\n");
}

void
argsetup(argcP, argvP)
    int *argcP;
    char ***argvP;
{
    int argc;
    char **argv, **curarg;
    char *eopts;
    char *envopts;
    int neopts;
    char *start, *end;
    arge *head, *tail, *cur;

    /* if no options specified in environment, we're done */
    if ((envopts = getenv("PGOPTION")) == (char *) NULL)
	return;

    if ((eopts = (char *) malloc(strlen(envopts) + 1)) == (char *) NULL) {
	fprintf(stderr, "cannot malloc copy space for PGOPTION\n");
	fflush(stderr);
	exit (2);
    }

    (void) strcpy(eopts, envopts);

    /*
     *  okay, we have PGOPTION from the environment, and we want to treat
     *  them as user-specified options.  to do this, we construct a new
     *  argv that has argv[0] followed by the arguments from the environment
     *  followed by the arguments on the command line.
     */

    head = cur = (arge *) NULL;
    neopts = 0;

    for (;;) {
	while (isspace(*eopts) && *eopts)
	    eopts++;

	if (*eopts == '\0')
	    break;

	if ((cur = (arge *) malloc(sizeof(arge))) == (arge *) NULL) {
	    fprintf(stderr, "cannot malloc space for arge\n");
	    fflush(stderr);
	    exit (2);
	}

	end = start = eopts;

	if (*start == '"') {
	    start++;
	    while (*++end != '\0' && *end != '"')
		continue;
	    if (*end == '\0') {
		fprintf(stderr, "unterminated string constant in env var PGOPTION\n");
		fflush(stderr);
		exit (2);
	    }
	    eopts = end + 1;
	} else if (*start == '\'') {
	    start++;
	    while (*++end != '\0' && *end != '\'')
		continue;
	    if (*end == '\0') {
		fprintf(stderr, "unterminated string constant in env var PGOPTION\n");
		fflush(stderr);
		exit (2);
	    }
	    eopts = end + 1;
	} else {
	    while (!isspace(*end) && *end)
		end++;
	    if (isspace(*end))
		eopts = end + 1;
	    else
		eopts = end;
	}

	if (head == (arge *) NULL) {
	    head = tail = cur;
	} else {
	    tail->a_next = cur;
	    tail = cur;
	}

	cur->a_arg = start;
	cur->a_next = (arge *) NULL;

	*end = '\0';
	neopts++;
    }

    argc = *argcP + neopts;

    if ((argv = (char **) malloc(argc * sizeof(char *))) == (char **) NULL) {
	fprintf(stderr, "can't malloc space for modified argv\n");
	fflush(stderr);
	exit (2);
    }

    curarg = argv;
    *curarg++ = *(*argvP)++;

    /* copy env args */
    while (head != (arge *) NULL) {
	cur = head;
	*curarg++ = head->a_arg;
	head = head->a_next;
	free(cur);
    }

    /* copy rest of args from command line */
    while (--(*argcP))
	*curarg++ = *(*argvP)++;

    /* all done */
    *argvP = argv;
    *argcP = argc;
}

void
handle_copy_out()
{
    bool copydone = false;
    char copybuf[COPYBUFSIZ];
    int ret;

    if (!Silent)
	fprintf(stdout, "Copy command returns...\n");
    
    while (!copydone) {
	ret = PQgetline(copybuf, COPYBUFSIZ);
	
	if (copybuf[0] == '.') {
	    copydone = true;	/* don't print this... */
	} else {
	    fputs(copybuf, stdout);
	    switch (ret) {
	    case EOF:
		copydone = true;
		/*FALLTHROUGH*/
	    case 0:
		fputc('\n', stdout);
		break;
	    case 1:
		break;
	    }
	}
    }
    fflush(stdout);
    PQendcopy();
}

void
handle_copy_in()
{
    bool copydone = false;
    bool firstload;
    bool linedone;
    char copybuf[COPYBUFSIZ];
    char *s;
    int buflen;
    int c;
    
    if (!Silent) {
	fputs("Enter info followed by a newline\n", stdout);
	fputs("End with a dot on a line by itself.\n", stdout);
    }
    
    /*
     * eat inevitable newline still in input buffer
     *
     * XXX the 'inevitable newline' is not always present
     *     for example `cat file | monitor -c "copy from stdin"'
     */
    fflush(stdin);
    if ((c = getc(stdin)) != '\n' && c != EOF) {
	(void) ungetc(c, stdin);
    }
    
    while (!copydone) {			/* for each input line ... */
	if (!Silent) {
	    fputs(">> ", stdout);
	    fflush(stdout);
	}
	firstload = true;
	linedone = false;
	while (!linedone) {		/* for each buffer ... */
	    s = copybuf;
	    buflen = COPYBUFSIZ;
	    for (; buflen > 1 &&
		 !(linedone = (c = getc(stdin)) == '\n' || c == EOF);
		 --buflen) {
		*s++ = c;
	    }
	    *s = '\0';
	    PQputline(copybuf);
	    if (firstload) {
		if (!strcmp(copybuf, ".")) {
		    copydone = true;
		}
		firstload = false;
	    }
	}
	PQputline("\n");
    }
    PQendcopy();
}
