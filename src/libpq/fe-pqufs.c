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
#include "fmgr.h"
#include "tmp/postgres.h"
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
	return 0;
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
	    
	    st = (LookupStat *)hash_search(p_statHashTPtr,resolve_path(path),
					   HASH_ENTER,&found);
	    bcopy(statbuf,&st->statbuf,sizeof(struct pgstat));
	    return 0;
	}
    }
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
	    dir = (LookupDir *)hash_search(p_dirHashTPtr,resolve_path(path),HASH_ENTER,&found);
	    dir->dir=  pdir;
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

/* Hash code stolen from utils/hash.
 * Until we can mix and match modules, I'm going to cut and paste.
 */

/*
 * dynahash.c -- dynamic hashing
 *
 * Identification:
 *	Header: /home/postgres/clarsen/postgres/newconf/RCS/dynahash.c,v 1.13 1992/08/18 18:27:16 mer Exp 
 */

/*
 * Fast arithmetic, relying on powers of 2,
 * and on pre-processor concatenation property
 */

# define MOD(x,y)		((x) & ((y)-1))

/*
 * external routines
 */

/*
 * Private function prototypes
 */
int *DynaHashAlloc ARGS((unsigned int size ));
void DynaHashFree ARGS((Pointer ptr ));
int hash_clear ARGS((HTAB *hashp ));
uint32 call_hash ARGS((HTAB *hashp , char *k , int len ));
SEG_OFFSET seg_alloc ARGS((HTAB *hashp ));
int bucket_alloc ARGS((HTAB *hashp ));
int my_log2 ARGS((int num ));

/* ----------------
 * memory allocation routines
 *
 * for postgres: all hash elements have to be in
 * the global cache context.  Otherwise the postgres
 * garbage collector is going to corrupt them. -wei
 *
 * ??? the "cache" memory context is intended to store only
 *     system cache information.  The user of the hashing
 *     routines should specify which context to use or we
 *     should create a separate memory context for these
 *     hash routines.  For now I have modified this code to
 *     do the latter -cim 1/19/91
 * ----------------
 */

int *
DynaHashAlloc(size)
    unsigned int size;
{
    return (int *) palloc(size);
}

void
DynaHashFree(ptr)
    Pointer ptr;
{
    pfree(ptr);
}

#define MEM_ALLOC	DynaHashAlloc
#define MEM_FREE	DynaHashFree

/* ----------------
 * Internal routines
 * ----------------
 */

static int expand_table();
static int hdefault();
static int init_htab();


/*
 * pointer access macros.  Shared memory implementation cannot
 * store pointers in the hash table data structures because 
 * pointer values will be different in different address spaces.
 * these macros convert offsets to pointers and pointers to offsets.
 * Shared memory need not be contiguous, but all addresses must be
 * calculated relative to some offset (segbase).
 */

#define GET_SEG(hp,seg_num)\
  (SEGMENT) (((unsigned int) (hp)->segbase) + (hp)->dir[seg_num])

#define GET_BUCKET(hp,bucket_offs)\
  (ELEMENT *) (((unsigned int) (hp)->segbase) + bucket_offs)

#define MAKE_HASHOFFSET(hp,ptr)\
  ( ((unsigned int) ptr) - ((unsigned int) (hp)->segbase) )

# if HASH_STATISTICS
static long hash_accesses, hash_collisions, hash_expansions;
# endif

/************************** CREATE ROUTINES **********************/

static HTAB *
hash_create( nelem, info, flags )
int	nelem;
HASHCTL *info;
int	flags;
{
	register HHDR *	hctl;
	HTAB * 		hashp;
	

	hashp = (HTAB *) MEM_ALLOC((unsigned int) sizeof(HTAB));
	bzero(hashp,sizeof(HTAB));

	if ( flags & HASH_FUNCTION ) {
	  hashp->hash    = info->hash;
	} else {
	  /* default */
	  hashp->hash	 = string_hash;
	}

	if ( flags & HASH_SHARED_MEM )  {
	  /* ctl structure is preallocated for shared memory tables */

	  hashp->hctl     = (HHDR *) info->hctl;
	  hashp->segbase  = (char *) info->segbase; 
	  hashp->alloc    = info->alloc;
	  hashp->dir 	  = (SEG_OFFSET *)info->dir;

	  /* hash table already exists, we're just attaching to it */
	  if (flags & HASH_ATTACH) {
	    return(hashp);
	  }

	} else {
	  /* setup hash table defaults */

	  hashp->alloc	  = MEM_ALLOC;
	  hashp->dir	  = NULL;
	  hashp->segbase  = NULL;

	}

	if (! hashp->hctl) {
	  hashp->hctl = (HHDR *) hashp->alloc((unsigned int)sizeof(HHDR));
	  if (! hashp->hctl) {
	    return(0);
	  }
	}
	  
	if ( !hdefault(hashp) ) return(0);
	hctl = hashp->hctl;
#ifdef HASH_STATISTICS
	hctl->accesses = hctl->collisions = 0;
#endif

	if ( flags & HASH_BUCKET )   {
	  hctl->bsize   = info->bsize;
	  hctl->bshift  = my_log2(info->bsize);
	}
	if ( flags & HASH_SEGMENT )  {
	  hctl->ssize   = info->ssize;
	  hctl->sshift  = my_log2(info->ssize);
	}
	if ( flags & HASH_FFACTOR )  {
	  hctl->ffactor = info->ffactor;
	}

	/*
	 * SHM hash tables have fixed maximum size (allocate
	 * a maximal sized directory).
	 */
	if ( flags & HASH_DIRSIZE )  {
	  hctl->max_dsize = my_log2(info->max_size);
	  hctl->dsize     = my_log2(info->dsize);
	}
	/* hash table now allocates space for key and data
	 * but you have to say how much space to allocate 
	 */
	if ( flags & HASH_ELEM ) {
	  hctl->keysize    = info->keysize; 
	  hctl->datasize   = info->datasize;
	}
	if ( flags & HASH_ALLOC )  {
	  hashp->alloc = info->alloc;
	}

	if ( init_htab (hashp, nelem ) ) {
	  hash_destroy(hashp);
	  return(0);
	}
	return(hashp);
}

/*
	Allocate and initialize an HTAB structure 
*/
static int
hdefault(hashp)
HTAB *	hashp;
{
  HHDR	*hctl;

  bzero(hashp->hctl,sizeof(HHDR));

  hctl = hashp->hctl;
  hctl->bsize    = DEF_BUCKET_SIZE;
  hctl->bshift   = DEF_BUCKET_SHIFT;
  hctl->ssize    = DEF_SEGSIZE;
  hctl->sshift   = DEF_SEGSIZE_SHIFT;
  hctl->dsize    = DEF_DIRSIZE;
  hctl->ffactor  = DEF_FFACTOR;
  hctl->nkeys    = 0;
  hctl->nsegs    = 0;

  /* I added these MS. */

  /* default memory allocation for hash buckets */
  hctl->keysize	 = sizeof(char *);
  hctl->datasize = sizeof(char *);

  /* table has no fixed maximum size */
  hctl->max_dsize = NO_MAX_DSIZE;

  /* garbage collection for HASH_REMOVE */
  hctl->freeBucketIndex = INVALID_INDEX;

  return(1);
}


static int
init_htab (hashp, nelem )
HTAB *	hashp;
int	nelem;
{
  register SEG_OFFSET	*segp;
  register int nbuckets;
  register int nsegs;
  int	l2;
  HHDR	*hctl;

  hctl = hashp->hctl;
 /*
  * Divide number of elements by the fill factor and determine a desired
  * number of buckets.  Allocate space for the next greater power of
  * two number of buckets
  */
  nelem = (nelem - 1) / hctl->ffactor + 1;

  l2 = my_log2(nelem);
  nbuckets = 1 << l2;

  hctl->max_bucket = hctl->low_mask = nbuckets - 1;
  hctl->high_mask = (nbuckets << 1) - 1;

  nsegs = (nbuckets - 1) / hctl->ssize + 1;
  nsegs = 1 << my_log2(nsegs);

  if ( nsegs > hctl->dsize ) {
    hctl->dsize  = nsegs;
  }

  /* Use two low order bits of points ???? */
  /*
    if ( !(hctl->mem = bit_alloc ( nbuckets )) ) return(-1);
    if ( !(hctl->mod = bit_alloc ( nbuckets )) ) return(-1);
   */

  /* allocate a directory */
  if (!(hashp->dir)) {
    hashp->dir = 
      (SEG_OFFSET *)hashp->alloc(hctl->dsize * sizeof(SEG_OFFSET));
    if (! hashp->dir)
      return(-1);
  }

  /* Allocate initial segments */
  for (segp = hashp->dir; hctl->nsegs < nsegs; hctl->nsegs++, segp++ ) {
    *segp = seg_alloc(hashp);
    if ( *segp == NULL ) {
      hash_destroy(hashp);
      return (0);
    }
  }

# if HASH_DEBUG
  fprintf(stderr, "%s\n%s%x\n%s%d\n%s%d\n%s%d\n%s%d\n%s%d\n%s%d\n%s%d\n%s%x\n%s%x\n%s%d\n%s%d\n",
		"init_htab:",
		"TABLE POINTER   ", hashp,
		"BUCKET SIZE     ", hctl->bsize,
		"BUCKET SHIFT    ", hctl->bshift,
		"DIRECTORY SIZE  ", hctl->dsize,
		"SEGMENT SIZE    ", hctl->ssize,
		"SEGMENT SHIFT   ", hctl->sshift,
		"FILL FACTOR     ", hctl->ffactor,
		"MAX BUCKET      ", hctl->max_bucket,
		"HIGH MASK       ", hctl->high_mask,
		"LOW  MASK       ", hctl->low_mask,
		"NSEGS           ", hctl->nsegs,
		"NKEYS           ", hctl->nkeys );
# endif
	return (0);
}

/********************** DESTROY ROUTINES ************************/

static hash_clear( hashp )
HTAB 	*hashp;
{
  fprintf(stderr,"hash_clear not implemented\n");
}


static void
hash_destroy ( hashp )
HTAB	*hashp;
{
  /* cannot destroy a shared memory hash table */
  Assert(! hashp->segbase);

  if (hashp != NULL) {
    register SEG_OFFSET 	segNum;
    SEGMENT		segp;
    int			nsegs = hashp->hctl->nsegs;
    int 		j;
    BUCKET_INDEX	*elp,p,q;
    ELEMENT		*curr;

    for (segNum =  0;  nsegs > 0; nsegs--, segNum++) {

      segp = GET_SEG(hashp,segNum);
      for (j = 0, elp = segp; j < hashp->hctl->ssize; j++, elp++) {
	for ( p = *elp; p != INVALID_INDEX; p = q ){
	  curr = GET_BUCKET(hashp,p);
	  q = curr->next;
	  MEM_FREE((char *) curr);
	}
      }
      free((char *)segp);
    }
    (void) MEM_FREE( (char *) hashp->dir);
    (void) MEM_FREE( (char *) hashp->hctl);
    hash_stats("destroy",hashp);
    (void) MEM_FREE( (char *) hashp);
  }
}

static void
hash_stats(where,hashp)
char *where;
HTAB *hashp;
{
# if HASH_STATISTICS

    fprintf(stderr,"%s: this HTAB -- accesses %ld collisions %ld\n",
		where,hashp->hctl->accesses,hashp->hctl->collisions);
	
    fprintf(stderr,"hash_stats: keys %ld keysize %ld maxp %d segmentcount %d\n",
	    hashp->hctl->nkeys, hashp->hctl->keysize,
	    hashp->hctl->max_bucket, hashp->hctl->nsegs);
    fprintf(stderr,"%s: total accesses %ld total collisions %ld\n",
	    where, hash_accesses, hash_collisions);
    fprintf(stderr,"hash_stats: total expansions %ld\n",
	    hash_expansions);

# endif

}

/*******************************SEARCH ROUTINES *****************************/

static uint32
call_hash ( hashp, k, len )
HTAB	*hashp;
char    *k;
int     len;
{
        int     hash_val, bucket;
	HHDR	*hctl;

	hctl = hashp->hctl;
        hash_val = hashp->hash(k, len);

        bucket = hash_val & hctl->high_mask;
        if ( bucket > hctl->max_bucket ) {
            bucket = bucket & hctl->low_mask;
        }

        return(bucket);
}

/*
 * hash_search -- look up key in table and perform action
 *
 * action is one of HASH_FIND/HASH_ENTER/HASH_REMOVE
 *
 * RETURNS: NULL if table is corrupted, a pointer to the element
 * 	found/removed/entered if applicable, TRUE otherwise.
 *	foundPtr is TRUE if we found an element in the table 
 *	(FALSE if we entered one).
 */
static int *
hash_search(hashp, keyPtr, action, foundPtr)
HTAB		*hashp;
char		*keyPtr;
HASHACTION	action;		/*
				 * HASH_FIND / HASH_ENTER / HASH_REMOVE
				 * HASH_FIND_SAVE / HASH_REMOVE_SAVED
				 */
Boolean	*foundPtr;
{
	uint32 bucket;
	int segment_num;
	int segment_ndx;
	SEGMENT segp;
	register ELEMENT *curr;
	HHDR	*hctl;
	BUCKET_INDEX	currIndex;
	BUCKET_INDEX	*prevIndexPtr;
	char *		destAddr;
	static struct State {
		ELEMENT      *currElem;
		BUCKET_INDEX currIndex;
		BUCKET_INDEX *prevIndex;
	} saveState;

	Assert((hashp && keyPtr));
	Assert((action == HASH_FIND) || (action == HASH_REMOVE) || (action == HASH_ENTER) || (action == HASH_FIND_SAVE) || (action == HASH_REMOVE_SAVED));

	hctl = hashp->hctl;

# if HASH_STATISTICS
	hash_accesses++;
	hashp->hctl->accesses++;
# endif
	if (action == HASH_REMOVE_SAVED)
	{
	    curr = saveState.currElem;
	    currIndex = saveState.currIndex;
	    prevIndexPtr = saveState.prevIndex;
	    /*
	     * Try to catch subsequent errors
	     */
	    Assert(saveState.currElem && !(saveState.currElem = 0));
	}
	else
	{
	    bucket = call_hash(hashp, keyPtr, hctl->keysize);
	    segment_num = bucket >> hctl->sshift;
	    segment_ndx = bucket & ( hctl->ssize - 1 );

	    segp = GET_SEG(hashp,segment_num);

	    Assert(segp);

	    prevIndexPtr = &segp[segment_ndx];
	    currIndex = *prevIndexPtr;
 /*
  * Follow collision chain
  */
	    for (curr = NULL;currIndex != INVALID_INDEX;) {
	  /* coerce bucket index into a pointer */
	      curr = GET_BUCKET(hashp,currIndex);

	      if (! strcmp((char *)&(curr->key), keyPtr)) {
	        break;
	      } 
	      prevIndexPtr = &(curr->next);
	      currIndex = *prevIndexPtr;
# if HASH_STATISTICS
	      hash_collisions++;
	      hashp->hctl->collisions++;
# endif
	    }
	}

	/*
	 *  if we found an entry or if we weren't trying
	 * to insert, we're done now.
	 */
	*foundPtr = (Boolean) (currIndex != INVALID_INDEX);
	switch (action) {
	case HASH_ENTER:
	  if (currIndex != INVALID_INDEX)
	    return(&(curr->key));
	  break;
	case HASH_REMOVE:
	case HASH_REMOVE_SAVED:
	  if (currIndex != INVALID_INDEX) {
	    Assert(hctl->nkeys > 0);
	    hctl->nkeys--;

	    /* add the bucket to the freelist for this table.  */
	    *prevIndexPtr = curr->next;
	    curr->next = hctl->freeBucketIndex;
	    hctl->freeBucketIndex = currIndex;

	    /* better hope the caller is synchronizing access to
	     * this element, because someone else is going to reuse
	     * it the next time something is added to the table
	     */
	    return (&(curr->key));
	  }
	  return((int *) TRUE);
	case HASH_FIND:
	  if (currIndex != INVALID_INDEX)
	    return(&(curr->key));
	  return((int *)TRUE);
	case HASH_FIND_SAVE:
	  if (currIndex != INVALID_INDEX)
	  {
	      saveState.currElem = curr;
	      saveState.prevIndex = prevIndexPtr;
	      saveState.currIndex = currIndex;
	      return(&(curr->key));
	  }
	  return((int *)TRUE);
	default:
	  /* can't get here */
	  return (NULL);
	}

	/* 
	    If we got here, then we didn't find the element and
	    we have to insert it into the hash table
	*/
	Assert(currIndex == INVALID_INDEX);

	/* get the next free bucket */
	currIndex = hctl->freeBucketIndex;
	if (currIndex == INVALID_INDEX) {

	  /* no free elements.  allocate another chunk of buckets */
	  if (! bucket_alloc(hashp)) {
	    return(NULL);
	  }
	  currIndex = hctl->freeBucketIndex;
	}
	Assert(currIndex != INVALID_INDEX);

	curr = GET_BUCKET(hashp,currIndex);
	hctl->freeBucketIndex = curr->next;

	  /* link into chain */
	*prevIndexPtr = currIndex;	

 	 /* copy key and data */
	destAddr = (char *) &(curr->key);
	strcpy(destAddr,keyPtr);
	curr->next = INVALID_INDEX;

	/* let the caller initialize the data field after 
	 * hash_search returns.
	 */
/*	bcopy(keyPtr,destAddr,hctl->keysize+hctl->datasize);*/

        /*
         * Check if it is time to split the segment
         */
	if (++hctl->nkeys / (hctl->max_bucket+1) > hctl->ffactor) {
/*
	  fprintf(stderr,"expanding on '%s'\n",keyPtr);
	  hash_stats("expanded table",hashp);
*/
	  if (! expand_table(hashp))
	    return(NULL);
	}
	return (&(curr->key));
}

/*
 * hash_seq -- sequentially search through hash table and return
 *             all the elements one by one, return NULL on error and
 *	       return TRUE in the end.
 *
 */
static int *
hash_seq(hashp)
HTAB		*hashp;
{
    static uint32 curBucket = 0;
    static ELEMENT *curElem = NULL;
    int segment_num;
    int segment_ndx;
    SEGMENT segp;
    HHDR *hctl;
    BUCKET_INDEX currIndex;
    BUCKET_INDEX *prevIndexPtr;
    char *destAddr;

    if (hashp == NULL)
    {
	/*
	 * reset static state
	 */
	curBucket = 0;
	curElem = NULL;
	return NULL;
    }

    hctl = hashp->hctl;
    for (; curBucket <= hctl->max_bucket; curBucket++) {
	segment_num = curBucket >> hctl->sshift;
	segment_ndx = curBucket & ( hctl->ssize - 1 );

	segp = GET_SEG(hashp, segment_num);

	if (segp == NULL)
	    return NULL;
	if (curElem == NULL)
	    prevIndexPtr = &segp[segment_ndx];
	else
	    prevIndexPtr = &(curElem->next);
	currIndex = *prevIndexPtr;
	if (currIndex == INVALID_INDEX) {
	    curElem = NULL;
	    continue;
	  }
	curElem = GET_BUCKET(hashp, currIndex);
	return(&(curElem->key));
      }
    return (int*)TRUE;
}


/********************************* UTILITIES ************************/
static int
expand_table(hashp)
HTAB *	hashp;
{
  	HHDR	*hctl;
	SEGMENT	old_seg,new_seg;
	int	old_bucket, new_bucket;
	int	new_segnum, new_segndx;
	int	old_segnum, old_segndx;
	int	dirsize;
	ELEMENT	*chain;
	BUCKET_INDEX *old,*newbi;
	register BUCKET_INDEX chainIndex,nextIndex;

#ifdef HASH_STATISTICS
	hash_expansions++;
#endif

	hctl = hashp->hctl;
	new_bucket = ++hctl->max_bucket;
	old_bucket = (hctl->max_bucket & hctl->low_mask);

	new_segnum = new_bucket >> hctl->sshift;
	new_segndx = MOD ( new_bucket, hctl->ssize );

	if ( new_segnum >= hctl->nsegs ) {
	  
	  /* Allocate new segment if necessary */
	  if (new_segnum >= hctl->dsize) {
	    (void) dir_realloc(hashp);
	  }
	  if (! (hashp->dir[new_segnum] = seg_alloc(hashp))) {
	    return (0);
	  }
	  hctl->nsegs++;
	}


	if ( new_bucket > hctl->high_mask ) {
	    /* Starting a new doubling */
	    hctl->low_mask = hctl->high_mask;
	    hctl->high_mask = new_bucket | hctl->low_mask;
	}

	/*
	 * Relocate records to the new bucket
	 */
	old_segnum = old_bucket >> hctl->sshift;
	old_segndx = MOD(old_bucket, hctl->ssize);

        old_seg = GET_SEG(hashp,old_segnum);
        new_seg = GET_SEG(hashp,new_segnum);

	old = &old_seg[old_segndx];
	newbi = &new_seg[new_segndx];
	for (chainIndex = *old; 
	     chainIndex != INVALID_INDEX;
	     chainIndex = nextIndex){

	  chain = GET_BUCKET(hashp,chainIndex);
	  nextIndex = chain->next;
	  if ( call_hash(hashp,
			 (char *)&(chain->key),
			 hctl->keysize) == old_bucket ) {
		*old = chainIndex;
		old = &chain->next;
	  } else {
		*newbi = chainIndex;
		newbi = &chain->next;
	  }
	    chain->next = INVALID_INDEX;
	}
	return (1);
}


static int
dir_realloc ( hashp )
HTAB *	hashp;
{
	register char	*p;
	char	**p_ptr;
	int	old_dirsize;
	int	new_dirsize;


	if (hashp->hctl->max_dsize != NO_MAX_DSIZE) 
	  return (0);

	/* Reallocate directory */
	old_dirsize = hashp->hctl->dsize * sizeof ( SEGMENT * );
	new_dirsize = old_dirsize << 1;

	p_ptr = (char **) hashp->dir;
	p = (char *) hashp->alloc((unsigned int) new_dirsize );
	if (p != NULL) {
	  bcopy ( *p_ptr, p, old_dirsize );
	  bzero ( *p_ptr + old_dirsize, new_dirsize-old_dirsize );
	  (void) free( (char *)*p_ptr);
	  *p_ptr = p;
	  hashp->hctl->dsize = new_dirsize;
	  return(1);
	} 
	return (0);

}


SEG_OFFSET
seg_alloc(hashp)
HTAB * hashp;
{
  SEGMENT segp;
  SEG_OFFSET segOffset;
  

  segp = (SEGMENT) hashp->alloc((unsigned int) 
	    sizeof(SEGMENT)*hashp->hctl->ssize);

  if (! segp) {
    return(0);
  }

  bzero((char *)segp,
	(int) sizeof(SEGMENT)*hashp->hctl->ssize);

  segOffset = MAKE_HASHOFFSET(hashp,segp);
  return(segOffset);
}

/*
 * allocate some new buckets and link them into the free list
 */
bucket_alloc(hashp)
HTAB *hashp;
{
  int i;
  ELEMENT *tmpBucket;
  int bucketSize;
  BUCKET_INDEX tmpIndex,lastIndex;

  bucketSize = 
    sizeof(BUCKET_INDEX) + hashp->hctl->keysize + hashp->hctl->datasize;

  /* make sure its aligned correctly */
  bucketSize += sizeof(int *) - (bucketSize % sizeof(int *));

  /* tmpIndex is the shmem offset into the first bucket of
   * the array.
   */
  tmpBucket = (ELEMENT *)
    hashp->alloc((unsigned int) BUCKET_ALLOC_INCR*bucketSize);

  if (! tmpBucket) {
    return(0);
  }

  tmpIndex = MAKE_HASHOFFSET(hashp,tmpBucket);

  /* set the freebucket list to point to the first bucket */
  lastIndex = hashp->hctl->freeBucketIndex;
  hashp->hctl->freeBucketIndex = tmpIndex;

  /* initialize each bucket to point to the one behind it */
  for (i=0;i<(BUCKET_ALLOC_INCR-1);i++) {
    tmpBucket = GET_BUCKET(hashp,tmpIndex);
    tmpIndex += bucketSize;
    tmpBucket->next = tmpIndex;
  }
  
  /* the last bucket points to the old freelist head (which is
   * probably invalid or we wouldnt be here) 
   */
  tmpBucket->next = lastIndex;

  return(1);
}

/* calculate the log base 2 of num */
my_log2( num )
int	num;
{
    int		i = 1;
    int		limit;

    for ( i = 0, limit = 1; limit < num; limit = 2 * limit, i++ );
    return (i);
}

/*
	Assume that we've already split the bucket to which this
	key hashes, calculate that bucket, and check that in fact
	we did already split it.
*/
static int
string_hash(key,keysize)
char *	key;
int	keysize;
{
	int h;
	register unsigned char *k = (unsigned char *) key;

	h = 0;
	/*
	 * Convert string to integer
	 */
	while (*k)
		h = h * PRIME1 ^ (*k++ - ' ');
	h %= PRIME2;

	return (h);
}


static tag_hash(key,keysize)
int *	key;
int	keysize;
{
	register int h = 0;

	/*
	 * Convert tag to integer;  Use four byte chunks in a "jump table"
	 * to go a little faster.  Currently the maximum keysize is 16
	 * (mar 17 1992) I have put in cases for up to 24.  Bigger than
	 * this will resort to the old behavior of the for loop. (see the
	 * default case).
	 */
	switch (keysize)
	{
	    case 6*sizeof(int):
		h = h * PRIME1 ^ (*key);
		key++;
		/* fall through */

	    case 5*sizeof(int):
		h = h * PRIME1 ^ (*key);
		key++;
		/* fall through */

	    case 4*sizeof(int):
		h = h * PRIME1 ^ (*key);
		key++;
		/* fall through */

	    case 3*sizeof(int):
		h = h * PRIME1 ^ (*key);
		key++;
		/* fall through */

	    case 2*sizeof(int):
		h = h * PRIME1 ^ (*key);
		key++;
		/* fall through */

	    case sizeof(int):
		h = h * PRIME1 ^ (*key);
		key++;
		break;

	    default:
		for(; keysize > (sizeof(int)-1); keysize -= sizeof(int), key++)
		    h = h * PRIME1 ^ (*key);
		/*
		 * now let's grab the last few bytes of the tag if the tag
		 * has (size % 4) != 0 (which it sometimes will on a sun3).
		 */
		if (keysize)
		{
		    char *keytmp = (char *)key;

		    switch (keysize)
		    {
			case 3:
			    h = h * PRIME1 ^ (*keytmp);
			    keytmp++;
			    /* fall through */
			case 2:
			    h = h * PRIME1 ^ (*keytmp);
			    keytmp++;
			    /* fall through */
			case 1:
			    h = h * PRIME1 ^ (*keytmp);
			    break;
		    }
		}
		break;
	}

	h %= PRIME2;
	return (h);
}

/*
 * This is INCREDIBLY ugly, but fast.
 * We break the string up into 8 byte units.  On the first time
 * through the loop we get the "leftover bytes" (strlen % 8).
 * On every other iteration, we perform 8 HASHC's so we handle
 * all 8 bytes.  Essentially, this saves us 7 cmp & branch
 * instructions.  If this routine is heavily used enough, it's
 * worth the ugly coding
 */
static int
disk_hash(key)
char *key;
{
        register int n = 0;
	register char *str = key;
	register int len = strlen(key);
	register int loop;

#define HASHC   n = *str++ + 65599 * n

        if (len > 0) {
                loop = (len + 8 - 1) >> 3;

                switch(len & (8 - 1)) {
			case 0: do {		/* All fall throughs */
					HASHC;  
				case 7: HASHC;
				case 6: HASHC;  
				case 5: HASHC;
				case 4: HASHC;  
				case 3: HASHC;
				case 2: HASHC;  
				case 1: HASHC;
                        } while (--loop);
                }

        }
	return(n);
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
