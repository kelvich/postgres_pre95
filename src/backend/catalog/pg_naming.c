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
 * OID LOpathOID(char *fname,int mode);
 *     enter pathname in system relation if it doesn't exist, create
 *     intervening directories unless they can't be made due to component
 *     length, mode is ignored. 
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
 *    LOpathOID(char *fname,int mode);
 *      take FilenameToOID of fname; if it doesn't exist take basename
 *      of fname and recursively deal with it (if in the root dir,
 *      LOcreatOID will handle it).  Allocate a new OID, do a put of
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
#include <string.h>
#include "catalog/syscache.h"
#include "catalog/pg_proc.h"
#include "catalog/pg_naming.h"
#include "catalog/pg_lobj.h"
#include "utils/rel.h"
#include "utils/palloc.h"
#include "catalog/indexing.h"
#include "utils/log.h"
#include "access/heapam.h"
#include "tmp/libpq-fs.h"

/*#define NAMINGDB 1*/

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
    char *cp;
    OID parentOID;
    struct naming *namingStruct;
    NameData comp;

    /* get root directory tuple */
    namingTuple = SearchSysCacheTuple(NAMEREL,
				      (char *) ObjectIdGetDatum(RootOID),
				      (char *) NameGetDatum(RootFileName),
				      (char *) NULL, (char *) NULL);
    if (namingTuple == NULL)
	elog(WARN, "root directory \"%s\" does not exist", RootFileName);
    namingStruct = (struct naming *) GETSTRUCT(namingTuple);

    /* strtok must not be used by more than one active function at a time */
    strcpy(pathcomponents,fname);
    cp = strtok(pathcomponents,"/");

    /* look up a component at a time */
    while (cp != NULL) {
	(void) namestrcpy(&comp, cp);
	parentOID = namingStruct->ourid;
	namingTuple = SearchSysCacheTuple(NAMEREL,
					  (char *) ObjectIdGetDatum(parentOID),
					  (char *) NameGetDatum(&comp),
					  (char *) NULL, (char *) NULL);

	if (namingTuple == NULL) {
	    /* component does not exist */
	    return InvalidObjectId;
	}
	namingStruct = (struct naming *) GETSTRUCT(namingTuple);
	cp = strtok(NULL,"/");
    }

    /* we have the tuple corresponding to the last component in namingStruct */
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
    bool hasindex;
    Relation idescs[Num_pg_name_indices];
    NameData tmpname;
    
    for (i = 0 ; i < Natts_pg_naming; i++) {
	nulls[i] = ' ';
	values[i] = NULL;
    }
    
    /* name is a string that's "long enough" but DataFill expects a NameData */
    (void) strncpy(tmpname.data, name, sizeof(NameData));

    i = 0;
    values[i++] = (char *) &tmpname;
    values[i++] = (char *) ObjectIdGetDatum(ourid);
    values[i++] = (char *) ObjectIdGetDatum(parentID);
#if NAMINGDB
    elog(NOTICE,"CreateNameTuple: parent = %d,oid = %d,name = %s",
	 parentID,ourid,name);
#endif

    namingDesc = heap_openr(Name_pg_naming);
    if (hasindex = RelationGetRelationTupleForm(namingDesc)->relhasindex)
	CatalogOpenIndices(Num_pg_name_indices, Name_pg_name_indices,
			   &idescs[0]);

    tup = heap_formtuple(Natts_pg_naming,
			 &namingDesc->rd_att,
			 values,
			 nulls);
    heap_insert(namingDesc,tup,(double *)NULL);

    if (hasindex) {
	CatalogIndexInsert(idescs, Num_pg_name_indices, namingDesc, tup);
	CatalogCloseIndices(Num_pg_name_indices, &idescs[0]);
    }

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

    namingTuple = SearchSysCacheTuple(NAMEREL,
				      (char *) ObjectIdGetDatum(parentID),
				      (char *) NameGetDatum(name),
				      (char *) NULL, (char *) NULL);
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
/*
 * hartzell --- Sat May  1 16:13:58 1993
 * fixed so that it doesn't stuff null's into fname.
 */
oid LOcreatOID(fname,mode)
     char *fname;
     int mode; /* ignored */
{
    char *filename;
    int filename_len;
    oid return_oid;
    char *tailname;
    oid basedirOID;

    char *root = "/";

    filename_len = strlen(fname);
    filename = (char *)palloc(filename_len + 1);
    bcopy(fname, filename, filename_len);
    filename[filename_len] = '\0';
    tailname = rindex(filename,'/');
    Assert(tailname != NULL);
    *tailname = '\0';
    tailname++;
    if (filename[0] == '\0') {
	basedirOID = FilenameToOID(root);
    } else {
	basedirOID = FilenameToOID(filename);
    }
    if (basedirOID == InvalidObjectId) {
#if NAMINGDB
	elog(NOTICE,"LOcreat: %s doesn't exist",filename[0]?filename:root);
#endif
	pfree(filename);
	return InvalidObjectId; /* directories don't exist */
    } else {
	HeapTuple namingTuple;
    
	struct naming *namingStruct;
	/*
	 * get the tuple from the system cache
	 */
	namingTuple = SearchSysCacheTuple(NAMEREL,
					  (char *)
					  ObjectIdGetDatum(basedirOID),
					  (char *) NameGetDatum(tailname),
					  (char *) NULL, (char *) NULL);
	if (namingTuple == NULL) {
	    /* create a tuple, insert it into heap. return oid.
	     */
	    return_oid = CreateNewNameTuple(basedirOID,tailname);
	    pfree(filename);
	    return (return_oid);
	} else {
   	    pfree(filename);
	    return InvalidObjectId;		/* file already exists */
	}
    }
}

oid LOpathOID(fname, mode)
    char *fname;
    int mode; /* ignored */
{
	char *basename, *tailname;
	int nlen;
	oid baseOID, retOID;

	if ((retOID = FilenameToOID(fname)) != InvalidObjectId)
		return retOID;
	nlen = strlen(fname);
	basename = (char *)palloc(nlen + 1);
	bcopy(fname, basename, nlen);
	basename[nlen] = 0;

	tailname = rindex(basename,'/');
	if (tailname == NULL || (strlen(tailname) > sizeof(NameData)))
		goto fin;
	if (tailname == basename) {
		retOID = LOcreatOID(fname,mode);
		goto fin;
	}
	*tailname++ = 0;
	if ((baseOID = FilenameToOID(basename)) == InvalidObjectId &&
	    (baseOID = LOpathOID(basename, mode)) == InvalidObjectId)
		goto fin;
	retOID = CreateNewNameTuple(baseOID,tailname);
fin:
	pfree(basename);
	return retOID;
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
    Relation namreln;
    HeapScanDesc namscan;
    HeapTuple htup;
    ScanKeyEntryData skey;
    oid pathOID;
    int result;

    if (path[0] == '/' && path[1] == '\0') {
	return (0);
    }

    if ((pathOID = FilenameToOID(path)) == InvalidObjectId) {
	return (0);
    }

    namreln = heap_openr(Name_pg_naming);
    RelationSetLockForRead(namreln);

    ScanKeyEntryInitialize(&skey, 0x0, Anum_pg_naming_parent_oid,
			   ObjectIdEqualRegProcedure,
			   ObjectIdGetDatum(pathOID));
    namscan = heap_beginscan(namreln, false, NowTimeQual, 1, &skey);

    if (HeapTupleIsValid(htup = heap_getnext(namscan, false, (Buffer *) NULL)))
	result = 0;
    else
	result = 1;

    /* clean up */
    heap_endscan(namscan);
    heap_close(namreln);

    return (result);
}

/*XXX: This primitive is skew, since there are two possibilities: !exist, !dir
  --case */
int LOisdir(path)
     char *path;
{
    oid oidf;
    int ignoretype;

    if ((oidf = FilenameToOID(path)) == InvalidObjectId) {
	return(0);
    }
    return(LOassocOIDandLargeObjDesc(&ignoretype,oidf) == NULL);
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
