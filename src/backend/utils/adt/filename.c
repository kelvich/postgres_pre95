
#include <strings.h>
#include <stdio.h>
#include <pwd.h>

#include "utils/log.h"

#define MAXPATHLEN 256

/*
 * "$Header$"
 */
char *pg_username ARGS((void ));


char *
filename_in(file)

char *file;

{
    char *str, *getenv();
    int ind;

    str = (char *) palloc(MAXPATHLEN * sizeof(*str));
    str[0] = '\0';
    if (file[0] == '~') {
	if (file[1] == '\0' || file[1] == '/') {
	    /* Home directory */

	    char name[16], *p;
	    struct passwd *pw;

	    strcpy(name, pg_username());

	    if ((pw = getpwnam(name)) == NULL) {
		elog(WARN, "User %s is not a Unix user on the db server.",
		     name);
	    }

	    strcpy(str, pw->pw_dir);

	    ind = 1;
	} else {
	    /* Someone else's directory */
	    char name[16], *p;
	    struct passwd *pw;
	    int len;

	    if ((p = (char *) index(file, '/')) == NULL) {
		strcpy(name, file+1);
		len = strlen(name);
	    } else {
		len = (p - file) - 1;
		strncpy(name, file+1, len);
		name[len] = '\0';
	    }
	    /*printf("name: %s\n");*/
	    if ((pw = getpwnam(name)) == NULL) {
		elog(WARN, "No such user: %s\n", name);
		ind = 0;
	    } else {
		strcpy(str, pw->pw_dir);
		ind = len + 1;
	    }
	}
    } else if (file[0] == '$') {  /* $POSTGRESHOME, etc.  expand it. */
	char name[16], environment[80], *envirp, *p;
	int len;

	if ((p = (char *) index(file, '/')) == NULL) {
		strcpy(environment, file+1);
		len = strlen(environment);
	} else {
		len = (p - file) - 1;
		strncpy(environment, file+1, len);
		environment[len] = '\0';
	}
	envirp = getenv(environment);
	if (envirp) {
		strcpy(str, envirp);
		ind = len + 1;
	}
	else {
		elog(WARN,"Couldn't find %s in your environment", environment);
	}
    } else {
	ind = 0;
    }
    strcat(str, file+ind);
    return(str);
}

char *
filename_out(s)

char *s;

{
	char *ret = (char *) palloc(strlen(s));

	strcpy(ret, s);
	return(ret);
}
