/*#define NAMINGDB 1*/
/* ----------------------------------------------------------------
 *   FILE
 *      pg_naming.c
 *
 *   DESCRIPTION
 *      routines to support manipulation of the pg_naming relation
 *
 *   INTERFACE ROUTINES
 * OID FilenameToOID(char *fname);
 * OID LOcreatOID(char *fname,int mode);
 * OID LOunlinkOID(char *fname);
 * int LOisdir(char *path);
 * int LOrename(char *path,char *newpath);
 *
 *   NOTES
 *
 *   IDENTIFICATION
 *      $Header$
 * ----------------------------------------------------------------
 */

/*
 * Wrappers for managing pathname to large object OID translation.
 *
 * OID FilenameToOID(char *fname);
 *     performs repeated queries on pathname pairs until no tuple found, or
 *     path is resolved.
 * OID LOcreatOID(char *fname,int mode);
 *     enter pathname in system relation.  error if directories don't exist.
 *     mode is ignored
 * OID LOunlinkOID(char *fname);
 *     remove dir/fname tuple.
 * int LOisdir(char *path);
 *     returns true if path is a directory (no OID mapping in 
 *     oid->large_object table).
 * int LOrename(char *path,char *newpath);
 *     rename path to newpath.  error if path doesn't exist, or basename of
 *     newpath doesn't exist.  If newpath is a directory, path becomes child 
 *     of that directory.  If basepath of newpath is a directory, then path 
 *     becomes child of basepath with name tailname of path.
 *
 * Schema:
 *    namingtable is <OID,Parent OID,file name component> keyed on the filename
 *    component and the parent OID.
 *
 * Description of algorithms:
 *    FilenameToOID given filename:
 *     Letting pi be path components, take <0,"/"> as key to find RootOID.
 *     call this RootOID = <0,"/"> for short.
 *     then take LargeObjectOID = <...<<RootOID,p0>,p1>,p2>,...>,pn>.
 *     if lookup fails at any point, return error code.
 *
 *     This seems slow, but this mapping should happen infrequently if we are 
 *     doing mostly read and write, not open.
 *
 *    LOcreatOID(char *fname,int mode);
 *      take basename of fname (last pathname component stripped off) and
 *      do filenametoOID on it.  BasedirOID = FilenameToOID(basename).  If it
 *      fails, then return error (no parent directory), else do lookup
 *      ExistingOID = <BasedirOID,tailname>.  if success, then return error
 *      (file already exists), else allocate a new OID, do a put of 
 *      <BasedirOID,fname,NewOID>.  return NewOID.
 *
 *    LOunlinkOID(char *fname);
 *      take basename of fname and do lookup BasedirOID =
 *         FilenameToOID(basename).
 *      If it fails, return error, else attempt delete of
 *          <BasedirOID,tailname>.
 *      If it fails, return error (no such file), else success.
 */

#include <stdio.h>
#include <strings.h>
#include "catalog/syscache.h"
#include "catalog/pg_naming.h"
#include "utils/rel.h"
#include "utils/log.h"
#include "access/heapam.h"
#include "tmp/libpq-fs.h"

#define RootOID         0        /* arbitrary. */
#define RootFileName   "/"

#define MAX_PATH_COMP 256

#ifndef MAXPATHNAME
#define MAXPATHNAME 1024
#endif


oid FilenameToOID(fname)
     char *fname;
{
    HeapTuple namingTuple;
    char pathcomponents[MAXPATHNAME];
    int n_comps;
    int i;

    struct naming *namingStruct;

    /* get root directory tuple */

    namingTuple = SearchSysCacheTuple(NAMEREL,RootOID,RootFileName);
    if (namingTuple == NULL) {
	/* complain about Root directory not existing */
/*	elog(DEBUG,"FilenameToOID: \"/\" NOT found\n");*/
	return InvalidObjectId;
    }
    namingStruct = (struct naming *) GETSTRUCT(namingTuple);

    /*
     * get the tuple from the system cache
     */
     strcpy(pathcomponents,fname);
    /* iterating over path components, lookup oids */
     {
	 char *cp;
	 /* be very careful, strtok must not be used by more than one active
	    function at a time */
	 cp = strtok(pathcomponents,"/");
	 while (cp != NULL) {
	     OID parentOID;
	     parentOID = namingStruct->ourid;
#if NAMINGDB
	     elog(NOTICE,"FilenameToOID: looking up \"%s\"\n",cp);
#endif
	     namingTuple = SearchSysCacheTuple(NAMEREL,parentOID,
					       cp);
	     if (namingTuple == NULL) {
		 /* complain about directory not existing */
		 return InvalidObjectId;
	     } else {
		 namingStruct = (struct naming *) GETSTRUCT(namingTuple);
	     }
	     cp = strtok(NULL,"/");
	 }
     }
#if 0
    for (i = 0; i < n_comps; i++) {
	OID parentOID;
	parentOID = namingStruct->ourid;

	namingTuple = SearchSysCacheTuple(NAMEREL,parentOID,
					  pathcomponents[i]);
	if (namingTuple == NULL) {
	    /* complain about directory not existing */
	    return InvalidObjectId;
	} else {
	    namingStruct = (struct naming *) GETSTRUCT(namingTuple);
	}
    }
#endif
    /* we have the tuple corresponding to the last component in namingStruct
     */
    return namingStruct->ourid;
}

void CreateNameTuple(parentID,name,ourid)
     oid parentID, ourid;
     char *name;
{
    char *values[Natts_pg_naming];
    char nulls[Natts_pg_naming];
    HeapTuple tup;
    Relation namingDesc;
    int i;
    
    for (i = 0 ; i < Natts_pg_naming; i++) {
	nulls[i] = ' ';
	values[i] = NULL;
    }
    
    i = 0;
    values[i++] = (char *) name;
    values[i++] = (char *) ourid;
    values[i++] = (char *)parentID;
#if NAMINGDB
    elog(NOTICE,"CreateNameTuple: parent = %d,oid = %d,name = %s",
	 parentID,ourid,name);
#endif

    namingDesc = heap_openr(Name_pg_naming);
    tup = heap_formtuple(Natts_pg_naming,
			 &namingDesc->rd_att,
			 values,
			 nulls);
    heap_insert(namingDesc,tup,(double *)NULL);

    pfree(tup);
    heap_close(namingDesc);

}

oid CreateNewNameTuple(parentID,name)
     oid parentID;
     char *name;
{
    oid thisoid;

    thisoid = newoid();
    CreateNameTuple(parentID,name,thisoid);
    
    return thisoid;
}

oid DeleteNameTuple(parentID,name)
     oid parentID;
     char *name;
{
    char *values[Natts_pg_naming];
    char nulls[Natts_pg_naming];
    HeapTuple tup;
    Relation namingDesc;
    HeapTuple namingTuple;
    struct naming *namingStruct;
    int i;
    oid thisoid;

    namingTuple = SearchSysCacheTuple(NAMEREL,parentID,name);
    if (namingTuple == NULL)
      return InvalidObjectId;
    namingDesc = heap_openr(Name_pg_naming);
#if NAMINGDB
    elog(NOTICE,"DeleteNameTuple: parent = %d,name = %s",
	 parentID,name);
#endif 

    heap_delete(namingDesc,&namingTuple->t_ctid);
    namingStruct = (struct naming *) GETSTRUCT(namingTuple);

    heap_close(namingDesc);
    return namingStruct->ourid;
}

/*
 * fname should always be an absolute path, e.g. starting with '/'.
 */
oid LOcreatOID(fname,mode)
     char *fname;
     int mode; /* ignored */
{
    char *tailname;
    oid basedirOID;
    /*void to_basename ARGS((char*,char*,char*));*/
    char *root = "/";

/*    to_basename(fname,basename,tailname);*/
    tailname = rindex(fname,'/');
    Assert(tailname != NULL);
    *tailname = '\0';
    tailname++;
    if (fname[0] == '\0') {
	basedirOID = FilenameToOID(root);
    } else {
	basedirOID = FilenameToOID(fname);
    }
    if (basedirOID == InvalidObjectId) {
#if NAMINGDB
	elog(NOTICE,"LOcreat: %s doesn't exist",fname[0]?fname:root);
#endif
	return InvalidObjectId; /* directories don't exist */
    } else {
	HeapTuple namingTuple;
    
	struct naming *namingStruct;
	/*
	 * get the tuple from the system cache
	 */
	namingTuple = SearchSysCacheTuple(NAMEREL,basedirOID,tailname);
	if (namingTuple == NULL) {
	    /* create a tuple, insert it into heap. return oid.
	     */
	    return CreateNewNameTuple(basedirOID,tailname);
	} else {
	    return InvalidObjectId;		/* file already exists */
	}
    }
}

int LOunlinkOID(fname)
     char *fname;
{
    char *tailname;
    oid basedirOID;
    if (fname[0]=='/' && fname[1] == '\0') {
	return InvalidObjectId;
    }
    tailname = rindex(fname,'/');
    Assert(tailname != NULL);
    *tailname++ = '\0';
    basedirOID = FilenameToOID(fname);
    if (basedirOID == InvalidObjectId) {
	return InvalidObjectId;
    } else {
	return DeleteNameTuple(basedirOID,tailname);
    }
}

int LOisemptydir(path)
     char *path;
{
    return 1; /* allow unlinking of non-empty directories for now */
}

/*XXX: This primitive is skew, since there are two possibilities: !exist, !dir
  --case */
int LOisdir(path)
     char *path;
{
    oid oidf;
    char ignoretype;
    oidf = FilenameToOID(path);

    if (oidf == InvalidObjectId)
      return 0;
    else
      return (LOassocOIDandLargeObjDesc(&ignoretype,oidf) == NULL);
}

int LOrename(path,newpath)
     char *path, *newpath;
{
    oid pathOID, newpathOID;
    char *pathtail, *newpathtail;

    pathtail = rindex(path,'/');
    newpathtail = rindex(newpath,'/');

    pathOID = FilenameToOID(path);
    if (pathOID == InvalidObjectId) /* file doesn't exist */
      return -PENOENT;

    newpathOID = FilenameToOID(newpath);
#if NAMINGDB
    elog(NOTICE,"rename %d to %d",pathOID,newpathOID);
#endif    
    if (newpathOID != InvalidObjectId) {
	if (!LOisdir(newpath))	/* newpath already exists, unlink it. */
	  LOunlinkOID(newpath);
	Assert(pathtail != NULL);
	CreateNameTuple(newpathOID,pathtail+1,pathOID);
	LOunlinkOID(path);
    } else {
	Assert(newpathtail != NULL);
	if (newpathtail != newpath) { /* "/a/b" */
	    *newpathtail = '\0';
	    if (!LOisdir(newpath))	/* above directories don't exist */
	      return -PENOENT;
	    CreateNameTuple(FilenameToOID(newpath),newpathtail+1,pathOID);
	    LOunlinkOID(path);
	    *newpathtail = '/';
	} else {
	    CreateNameTuple(FilenameToOID("/"),newpathtail+1,pathOID);
	    LOunlinkOID(path);
	}
    }
    return 0;
}

/*
 *
 * Support functions for this file
 *
 */
#if 0
/* break a filename into its path components */
/* Careful because buffer is shared */
int path_parse(pathname,pathcomp,n_comps)
     char *pathname;
     char *pathcomp[];
     int n_comps; /* size of pathcomp array */
{
    static char tmpbuf[2048];
    int i = 0;
    char *cp;

    strcpy(tmpbuf,pathname);

    pathcomp[0] = &tmpbuf[1]; /* assuming leading "/" */
    cp = &tmpbuf[1];

    if (*pathcomp[0] != '\0')
      i++;

    do {
	while(*cp && *cp != '/') /* skip non "/" */
	  cp++;
	if (*cp) {
	    *cp = '\0';
	    cp++;
	    if (*cp)   /* stuff after "/" becomes component name */
	      pathcomp[i++] = cp;
	    else
	      break;
	} else
	  break;
    } while (i < n_comps);
    return i;
}

/*
 * to_basename of "/foo" -> "/"
 * "/foo/bar" -> "/foo/"
 */
void to_basename(fname,bname,tname)
     char *fname, *bname, *tname;
{
    char *lastslash = NULL, *cp;
    strcpy(bname,fname);
    cp = bname;

    /* find last slash and truncate string */
    while(*cp != '\0') {
	if (*cp == '/')
	  lastslash = cp;
	cp++;
    }
    if (lastslash) *(lastslash+1) = '\0';

    /* copy into tailname, the stuff after the last slash */
    for(cp = fname+(lastslash+1-bname); *cp; cp++,tname++) {
	*tname = *cp;
    }
    *tname = '\0';
}
#endif
