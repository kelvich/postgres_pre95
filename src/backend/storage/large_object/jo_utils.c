/*
 * jo_utils.c  -  functions to help with Jaquith operations
 *
 * $Header: /data/01/postgres/src/backend/storage/large_object/RCS/jo_utils.c,v 
1.2 1992/02/28 05:33:28 boris Exp $
 */

#include <sys/file.h>
#include <strings.h>
#include "tmp/c.h"
#include "tmp/libpq-fs.h"
#include "access/relscan.h"
#include "access/tupdesc.h"
#include "catalog/pg_naming.h"
#include "catalog/pg_lobj.h"
#include "storage/itemptr.h"
#include "utils/rel.h"
#include "utils/large_object.h"
#include "utils/log.h"

char *JO_path(char *name) {      /* get the real path name */
    char *path, *jtemp;

    if (*name != '/')
	elog(WARN, "Invalid File Name: %s", name);
    jtemp = name;
    jtemp++;
    jtemp = strchr(jtemp, '/');
    path = (char *)palloc(strlen(jtemp) + 2); 
    strcpy(path, jtemp);
    if (!path) 
	elog(WARN, "Invalid File Name: %s", name);
    return path;
}


char *JO_arch(char *name) {
    char *jarch, *jtemp;

    jtemp = (char *)palloc(strlen(name) + 2);
    strcpy(jtemp, name);

    jtemp++;                /* find the slash after the first one */
    jarch = jtemp;
    while (*jtemp != '/')
	jtemp++;
    *jtemp = 0;
    jtemp = jarch;

    jarch = (char *)palloc(strlen(jtemp) + 2);
    jarch = strcpy(jarch, jtemp);

    jtemp--;
    pfree(jtemp);
    if (!jarch) 
	elog(WARN, "Invalid Archive name for Jaquith");

    return jarch;
}


int JO_get(char *name) {
    char sysbuf[160];
    int status;

    strcpy(sysbuf, "jget -arch ");
    strcat(sysbuf, JO_arch(name));
    strcat(sysbuf, " ");
    strcat(sysbuf, JO_path(name));	
    
    status = system(sysbuf);
    if (status != 0)
	elog(WARN, "Call to Jaquith Failed");
    return status;
}

int JO_put(char *name) {
    char sysbuf[160];
    int status;

    strcpy(sysbuf, "jput -arch ");
    strcat(sysbuf, JO_arch(name));
    strcat(sysbuf, " ");
    strcat(sysbuf, JO_path(name));	
    
    status = system(sysbuf);
    if (status != 0)
	elog(WARN, "Call to Jaquith Failed");
    return status;
}

int JO_clean(char *path) {

    unlink(path);  /* remove file from unix, only jaquith should have it */
}


