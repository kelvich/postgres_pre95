/* ----------------------------------------------------------------
 *   FILE
 *      fe-pqufs.c
 *
 *   DESCRIPTION
 *      Unix filesystem abstractions for large objects
 *
 *   SUPPORT ROUTINES
 *
 *   INTERFACE ROUTINES
 *      p_open,p_read,p_write,p_close
 *      p_creat, p_lseek, p_tell
 *      p_stat
 *      p_rename, p_mkdir, p_rmdir, p_unlink
 *      p_opendir, p_readdir, p_rewinddir, p_closedir
 *      p_chdir, p_getwd
 *      p_telldir, p_seekdir
 *      For description, see below and unix man(2) pages.
 *
 *   NOTES
 *      These routines are NOT compiled into the postgres backend,
 *      rather they end up in libpq.a.
 *
 *   IDENTIFICATION
 *      $Header$
 * ----------------------------------------------------------------
 */

/*
 * Unix abstractions for large objects.
 *
 * p_open, p_read, p_write, p_close
 *
 * p_creat, p_lseek, p_tell
 * p_ftruncate *
 *
 * p_stat (certain fields in the stat structure, e.g device, are
 *         meaningless.  Other fields, such as the times, are not valid yet.)
 *
 * p_rename, p_mkdir, p_rmdir, p_unlink
 *
 * p_opendir, p_readdir, p_rewinddir, p_closedir
 *
 *
 * These p_* calls are translated to fastpath function calls in backend's 
 * address space.  The large object file handles reside in, and the 
 * operations occur in backend's address space.
 *
 * These two calls are local functions that do not call the fastpath.  Each 
 * pathname in the p_* calls are translated to an absolute pathname using the
 * current working directory, initially "/".
 * p_chdir *
 * p_getwd *
 *
 * The semantics of the p_* calls are, for the most part, the same as the 
 * unix calls.
 *
 * Extent of the similarity with the Unix filesystem
 * -------------------------------------------------
 * There is no idea or support of a '.' or '..' file.  The functions opendir, 
 * readdir, telldir, seekdir, rewinddir and closedir are instead implemented 
 * as queries.
 */

#include "tmp/simplelists.h"
#include "tmp/libpq.h"
#include "tmp/libpq-fe.h"
#include "tmp/libpq-fs.h"
#include "tmp/postgres.h"
#include "fmgr.h"
#include "tmp/fastpath.h"

# include	"utils/hsearch.h"

#ifndef MAXPATHLEN
#define MAXPATHLEN 1024
#endif

/* cwdir set from PFCWD environment variable, if available, else default */
char cwdir[MAXPATHLEN] = "/";			/* current working directory */

/*
 * Forward declarations for functions used later in this module
 */
static PDIR *new_PDIR ARGS((void ));
static Direntry *new_Direntry ARGS((char *name , oid OIDf ));
static char *resolve_path ARGS((char *path ));

void PQufs_init();

int p_errno = 0;			/* error code */
int p_attr_caching = 0;  /* incorrect, but fast caching */

typedef struct { 
    char key[MAXPATHLEN];
    struct pgstat statbuf;
} LookupStat;
HTAB *p_statHashTPtr;

typedef struct { 
    char key[MAXPATHLEN];
    PDIR *dir;
} LookupDir;
HTAB *p_dirHashTPtr;

int p_open(fname,mode)
     char *fname;
     int mode;
{
    int fd;
    PQArgBlock argv[2];
    char *ret;

    argv[0].isint = 0;
    argv[0].len = VAR_LENGTH_ARG;
    argv[0].u.ptr = (int *)resolve_path(fname);

    argv[1].isint = 1;
    argv[1].len = 4;
    argv[1].u.integer = mode;
    
    ret = PQfn(F_LOOPEN,&fd,sizeof(int32),NULL,1,argv,2); 
    if (ret == NULL)
      return -1;
    else if (*ret == 'V')
      return -1;
    else {
	p_errno = fd>=0 ? 0 : -fd;

	/* have to do this to reset offset in shared fd cache */
	/* but only if fd is valid */
	if (fd >= 0 && p_lseek(fd, 0L, L_SET) < 0)
		return -1;

	return fd;
    }
}

int p_close(fd)
     int fd;
{
    PQArgBlock argv[1];
    char *pqret;
    int retval;

    argv[0].isint = 1;
    argv[0].len = 4;
    argv[0].u.integer = fd;
    pqret = PQfn(F_LOCLOSE,&retval,sizeof(int32),NULL,1,argv,1);
    if (pqret == NULL)
      return -1;
    else if (*pqret == 'V')
      return -1;
    else {
	p_errno = retval>=0 ? 0 : -retval;
	return retval;
    }
}

int p_read(fd, buf, len)
     int fd;
     char *buf;
     int len;
{
    return (PQfsread(fd, buf, len));
}

int p_write(fd, buf, len)
     int fd;
     char *buf;
     int len;
{
    return (PQfswrite(fd, buf, len));
}

int p_lseek(fd,offset,whence)
     int fd, offset, whence;
{
    PQArgBlock argv[3];
    char *pqret;
    int retval;
    
    argv[0].isint = 1;
    argv[0].len = 4;
    argv[0].u.integer = fd;
    
    argv[1].isint = 1;
    argv[1].len = 4;
    argv[1].u.integer = offset;

    argv[2].isint = 1;
    argv[2].len = 4;
    argv[2].u.integer = whence;

    pqret = PQfn(F_LOLSEEK,&retval,sizeof(int32),NULL,1,argv,3);

    if (pqret == NULL)
      return -1;
    else if (*pqret == 'V')
      return -1;
    else {
	p_errno = retval>=0 ? 0 : -retval;
	return retval;
    }
}

int p_creat(path,mode,objtype)
     char *path;
     int mode;
     int objtype;
{
    PQArgBlock argv[3];
    char *pqret;
    int retval;

    argv[0].isint = 0;
    argv[0].len = VAR_LENGTH_ARG;
    argv[0].u.ptr = (int *)resolve_path(path);

    argv[1].isint = 1;
    argv[1].len = 4;
    argv[1].u.integer = mode;

    argv[2].isint = 1;
    argv[2].len = 4;
    argv[2].u.integer = objtype;

    pqret = PQfn(F_LOCREAT,&retval,sizeof(int32),NULL,1,argv,3);

    if (pqret == NULL)
      return -1;
    else if (*pqret == 'V')
      return -1;
    else {
	p_errno = retval>=0 ? 0 : -retval;
	return retval;
    }
}

int p_tell(fd)
     int fd;
{
    int retval;
    PQArgBlock argv[1];
    char *pqret;

    argv[0].isint = 1;
    argv[0].len = 4;
    argv[0].u.integer = fd;

    pqret = PQfn(F_LOTELL,&retval,sizeof(int32),NULL,1,argv,1);

    if (pqret == NULL)
      return -1;
    else if (*pqret == 'V')
      return -1;
    else {
	p_errno = retval>=0 ? 0 : -retval;
	return retval;
    }
}

int p_mkdir(path,mode)
     char *path;
     int mode;
{
    PQArgBlock argv[2];
    char *pqret;
    int retval;

    argv[0].isint = 0;
    argv[0].len = VAR_LENGTH_ARG;
    argv[0].u.ptr = (int *)resolve_path(path);

    argv[1].isint = 1;
    argv[1].len = 4;
    argv[1].u.integer = mode;

    pqret = PQfn(F_LOMKDIR,&retval,sizeof(int32),NULL,1,argv,2);

    if (pqret == NULL)
      return -1;
    else if (*pqret == 'V')
      return -1;
    else {
	p_errno = retval>=0 ? 0 : -retval;
	return retval;
    }
}

int p_unlink(path)
     char *path;
{
    PQArgBlock argv[1];
    char *pqret;
    int retval;

    argv[0].isint = 0;
    argv[0].len = VAR_LENGTH_ARG;
    argv[0].u.ptr = (int *)resolve_path(path);

    pqret = PQfn(F_LOUNLINK,&retval,sizeof(int32),NULL,1,argv,1);

    if (pqret == NULL)
      return -1;
    else if (*pqret == 'V')
      return -1;
    else {
	p_errno = retval>=0 ? 0 : -retval;
	return retval;
    }
}

int p_rmdir(path)
     char *path;
{
    PQArgBlock argv[1];
    char *pqret;
    int retval;

    argv[0].isint = 0;
    argv[0].len = VAR_LENGTH_ARG;
    argv[0].u.ptr = (int *)resolve_path(path);

    pqret = PQfn(F_LORMDIR,&retval,sizeof(int32),NULL,1,argv,1);

    if (pqret == NULL)
      return -1;
    else if (*pqret == 'V')
      return -1;
    else {
	p_errno = retval>=0 ? 0 : -retval;
	return retval;
    }
}

int p_ferror(fd)
     int fd;
{
    return 0; /* no errors reported yet... */
}

int p_rename(path,pathnew)
     char *path, *pathnew;
{
    PQArgBlock argv[2];
    char *pqret;
    int retval;
    static char tmpdir[MAXPATHLEN];
    strcpy(tmpdir,resolve_path(path));
    argv[0].isint = 0;
    argv[0].len = VAR_LENGTH_ARG;
    argv[0].u.ptr = (int *)tmpdir;

    argv[1].isint = 0;
    argv[1].len = VAR_LENGTH_ARG;
    argv[1].u.ptr = (int *)resolve_path(pathnew);

    pqret = PQfn(F_LORENAME,&retval,sizeof(int32),NULL,1,argv,2);

    if (pqret == NULL)
      return -1;
    else if (*pqret == 'V')
      return -1;
    else {
	p_errno = retval>=0 ? 0 : -retval;
	return retval;
    }
}
/* XXX: for correct operation, we should flush all cached attributes
   when flushing directory contents.  We don't. */
int p_stat(path,statbuf)
     char *path;
     struct pgstat *statbuf;
{
    int stlen;
    PQArgBlock argv[2];
    struct varlena *statres;
    int vstatlen;
    char *pqret;
    int found = 0;
    LookupStat *st;
    char *key;

    PQufs_init();
    if (p_attr_caching &&
	(st = (LookupStat *)hash_search(p_statHashTPtr,resolve_path(path),HASH_FIND,&found)) && found) {
	bcopy(&st->statbuf,statbuf,sizeof(struct pgstat));
    } else {
	argv[0].isint = 0;
	argv[0].len = VAR_LENGTH_ARG;
	argv[0].u.ptr = (int *)resolve_path(path);

	/* PQfn deals with >4byte return values fine. */
	/*    vstatlen = sizeof(struct pgstat) + sizeof(int);
	      statres = (struct varlena *) palloc(vstatlen); */
	pqret = PQfn(F_LOSTAT,(int *)statbuf,sizeof(struct pgstat),&stlen,2,argv,1);
	/*    bcopy((char *) VARDATA(statres), (char *) statbuf, sizeof(struct pgstat));
	      pfree(statres); */

	if (stlen == 5) {
	    p_errno = PENOENT;
	    return -1;
	}

	if (pqret == NULL)
	  return -1;
	else if (*pqret == 'V')
	  return -1;
	else {
	    /* Since directories don't keep unix-like size info,
	       we have to fake it */
	    if (S_ISDIR(statbuf->st_mode)) {
		PDIR *p = p_opendir(path);
		struct pgdirent *dp;
		int entries = 0;
		for(dp = p_readdir(p); dp; dp = p_readdir(p)) {
		    entries++;
		}
		p_closedir(path);
		statbuf->st_size = 32*entries;
	    }
	    
	    if (p_attr_caching) {
		st = (LookupStat *)hash_search(p_statHashTPtr,
					       resolve_path(path),
					       HASH_ENTER,&found);
		bcopy(statbuf,&st->statbuf,sizeof(struct pgstat));
	    }

	}
    }
    return 0;
}


/*
 * open a portal to the query 
 * 'retrieve portal dir (pg_naming.filename) where pg_naming.parentid =
 * id-of-directory'
 */
PDIR *p_opendir(path)
     char *path;
{
    /* Get OID of directory (path) */
    int stlen;
    PQArgBlock argv[2];
    char *pqret;
    oid pathOID;
    LookupDir *dir;
    int found = 0;

    PQufs_init();
    if (p_attr_caching && 
	(dir = (LookupDir *)hash_search(p_dirHashTPtr,resolve_path(path),HASH_FIND,&found))
	&& found) {
	dir->dir->current_entry = (Direntry *)SLGetHead(&dir->dir->dirlist);
	return dir->dir;
    } else {

	argv[0].isint = 0;
	argv[0].len = VAR_LENGTH_ARG;
	argv[0].u.ptr = (int *)resolve_path(path);

	pqret = PQfn(F_FILENAMETOOID,(int *)&pathOID,sizeof(int32),NULL,1,argv,1);
	if (pqret == NULL)
	  return NULL;
	else if (*pqret == 'V')
	  return NULL;
	else {
	    PDIR *pdir = new_PDIR();
	    char query[512];
	    char *res;

#if 0
	    if ((res == NULL) || (*res != 'C')) { /* CBEGIN */
		return NULL;
	    }
#endif
	    sprintf(query,"retrieve (pg_naming.filename,pg_naming.ourid) where pg_naming.parentid = \"%d\"::oid",pathOID);
	    res = PQexec(query);
	    if ((res != NULL) && (*res == 'P')) /* CRETRIEVE */
	      {
		  PortalBuffer *p;
		  int k,g,t;
		  p = PQparray(++res);
		  g = PQngroups(p);
		  t = 0;
		  for (k=0;k<g;k++) {
		      int m,n,i;
		      n = PQntuplesGroup(p,k);
		      m = PQnfieldsGroup(p,k);
		      for (i =0;i<n;i++) {
			  char *name = PQgetvalue(p,t+i,0);
			  oid OIDf = atoi(PQgetvalue(p,t+i,1));
			  Direntry *d = new_Direntry(name,OIDf);
			  SLAddTail(&pdir->dirlist,&d->Node);
		      }
		      t += n;
		  }
	      }
	    pdir->current_entry = (Direntry *)SLGetHead(&pdir->dirlist);
	    if (p_attr_caching) {
		dir = (LookupDir *)hash_search(p_dirHashTPtr,resolve_path(path),HASH_ENTER,&found);
		dir->dir=  pdir;
	    }
	    return pdir;
	}
    }
}

struct pgdirent *p_readdir(dirp)
     PDIR *dirp;
{
    struct pgdirent *d = NULL;
    if (dirp->current_entry != NULL) {
	d = &dirp->current_entry->d;
	dirp->current_entry = (Direntry *)SLGetSucc(&dirp->current_entry->Node);
    }
    return d;
}

void p_rewinddir(dirp)
     PDIR *dirp;
{
    dirp->current_entry = (Direntry *)SLGetHead(&dirp->dirlist);
}

int p_telldir(dirp)
     PDIR *dirp;
{
    Direntry *d;
    int pos;
    d = (Direntry *)SLGetHead(&dirp->dirlist);
    for (pos = 0; d != dirp->current_entry; d = (Direntry *)SLGetSucc(&d->Node),pos ++)
      ;
    return pos;
}

void p_seekdir(dirp,pos)
     PDIR *dirp;
     int pos;
{
    Direntry *d;
    d = (Direntry *)SLGetHead(&dirp->dirlist);
    for (;pos > 0 && d != NULL; pos--,d = (Direntry *)SLGetSucc(&d->Node))
      ;
    dirp->current_entry = d;
}

int p_closedir(dirp)
     PDIR *dirp;
{
    /* well, how does one garbage collect these things? */
    /* We free it.  We don't in this case becuase we're trying to
       cache the stuff. We free with p_closedir_free when the cache is
       flushed.*/

    return 0;
}

void p_closedir_free(dirp)
     PDIR *dirp;
{
    Direntry *d, *dfree = NULL;
    d = (Direntry *)SLGetHead(&dirp->dirlist);
    for (;d; d=(Direntry *)SLGetSucc(&d->Node)) {
	if (dfree)
	  pfree(dfree);
	dfree = d;
    }
    pfree(dirp);
}

int p_chdir(path)
     char *path;
{
    char *evar;
    struct pgstat st;

    if (p_stat(path,&st) < 0) {
	return -1;
    }
    if (!S_ISDIR(st.st_mode)) {
	p_errno = PENOTDIR;
	return -1;
    }
    
    if (*path == '/') {
	strcpy(cwdir,path);
    } else {
	strcpy(cwdir,resolve_path(path));
    }
    evar = palloc(strlen(cwdir)+strlen("PFCWD")+2);
    sprintf(evar, "PFCWD=%s", cwdir);
    if (putenv(evar))
	return (-1);

    pfree(evar);
    return (0);
}

char *p_getwd(path)
     char *path;
{
    char *foo;
    foo = resolve_path("/"); 	/* initialize cwdir, if necessary */
    strcpy(path,cwdir);
    return path;
}

/*
 * ------------------------------------------------
 * Support routines for attr caching operations
 * ------------------------------------------------
 */
void p_dirflush(path)
     char *path;
{
    LookupDir *dir;
    int found = 0;
    if (p_attr_caching && 
	(dir = (LookupDir *)hash_search(p_dirHashTPtr,resolve_path(path),HASH_REMOVE,&found))
	&& found) {
	p_closedir_free(dir->dir); /* free space. */
    }
}

void p_dirflushparent(path)
     char *path;
{
    char pathname[2049];
    strcpy(pathname,path);
    strcat(pathname,"/..");
    p_dirflush(path);
}

/*
 * ------------------------------------------------
 * Support routines for DIR operations
 * ------------------------------------------------
 */
static PDIR *new_PDIR()
{
    PDIR *p;
    p = (PDIR *)palloc(sizeof(PDIR));
    SLNewList(&p->dirlist,offsetof(Direntry,Node));
    p->current_entry = NULL;
    return p;
}

static Direntry *new_Direntry(name,OIDf)
     char *name;
     oid OIDf;
{
    Direntry *d;
    d = (Direntry *)palloc(sizeof(Direntry));
    SLNewNode(&d->Node);
    d->d.d_ino = OIDf;
    d->d.d_namlen = strlen(name);
    strcpy(d->d.d_name,name);
    return d;
}
/* use by all the p_* routines to map relative to absolute path */
#define EOP(x) ((x)=='/' ||(x) =='\0')
#define DOT(x) ((x) =='.')

static int cwdinit = 0;

/* Be careful, the result returned is statically allocated*/
static char *resolve_path(gpath)
     char *gpath;
{
    char *cp;
    static char thenewdir[MAXPATHLEN];
    char tpath[MAXPATHLEN];
    char *path = tpath;
    strcpy(path,gpath);

    if (!cwdinit) {
	if ((cp = (char *)getenv("PFCWD")) != NULL)
		strcpy(cwdir,cp);
	cwdinit = 1;
    }
    if (*path == '/') {
	strcpy(thenewdir,path);
	return thenewdir;
    }
    strcpy(thenewdir,cwdir);
    while (path && *path) {
	if (DOT(path[0]) && DOT(path[1]) && EOP(path[2])) {
	    char *sep = (char *)rindex(thenewdir,'/');
	    if (path[2] == '/')
	      path += 3;
	    else
	      path += 2;
	    if (sep != NULL && sep != thenewdir) {
		*sep = '\0';
	    } else {
		thenewdir[0] = '/'; thenewdir[1] = '\0';
	    }
	    continue;
	}
	if (DOT(path[0]) && EOP(path[1])) {
	    if (path[1] == '/')
	      path += 2;
	    else
	      path += 1;
	    continue;
	}
	if (path[0]) {
	    char *comp = (char *)index(path,'/');
	    if (comp != NULL) {
		*comp='\0';
		if (thenewdir[1]) /* not simply "/" */
		  strcat(thenewdir,"/");
		strcat(thenewdir,path);
		path = comp+1;
	    } else {
		if (thenewdir[1]) /* not simply "/" */
		  strcat(thenewdir,"/");
		strcat(thenewdir,path);
		path = NULL; /* end of all the components */
	    }
	    continue;
	}
    }
    return thenewdir;
}

/*
 * Initializiation
 */

void PQufs_init() {
    HASHCTL info;
    int hash_flags;
    static int inited;
    if (inited) return;
    inited = 1;
    if (p_attr_caching) {
	info.keysize = MAXPATHLEN;
	info.datasize = sizeof(struct pgstat);
	info.hash = string_hash;
	hash_flags = HASH_ELEM | HASH_FUNCTION;
	p_statHashTPtr = hash_create(10,&info,hash_flags);

	info.keysize = MAXPATHLEN;
	info.datasize = sizeof(PDIR *);
	info.hash = string_hash;
	hash_flags = HASH_ELEM | HASH_FUNCTION;
	p_dirHashTPtr =  hash_create(10,&info,hash_flags);
    }
}
