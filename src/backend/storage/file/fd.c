/*
 * fd.c --
 *	Virtual file descriptor code.
 *
 * Note:
 *	Useful to get around the limit imposed be NOFILE (in stdio.h).
 *		(well, sys/param.h at least)
 *
 * Also note: this whole thing is UNIX dependent.  (well 70% anyway)
 *
 * NOTE: SEQUENT OS BUG: on open w/ O_CREAT if EMFILE error occurs the
 * file is *still* created in the directory.  Therefore, we must ensure
 * fd descriptors are available before calling open if O_CREAT flag is
 * set.
 */

/*
 * CURRENT HACK
 *
 * Problem: Postgres does a system(ld...) to do dynamic loading.  This
 * will open several extra files in addition to those used by Postgres.
 * We need to do this hack to guarentee that there are file descriptors free
 * for ld to use.
 *
 * The current solution is to limit the number of files descriptors
 * that this code will allocated at one time.  (it leaves RESERVE_FOR_LD
 * free)
 */

#include <sys/file.h>
#include <sys/param.h>
#include <errno.h>
#include <sys/stat.h>
#if defined(PORTNAME_bsd44)
	/* 
	 * XXX truely bogus move on 44's part - st_?time are now #defined.
	 * So we do the expansion so as not to screw up pgstat structure.
	 */
#undef st_atime
#undef st_ctime
#undef st_mtime
#endif

extern errno;

#include "tmp/c.h"
#include "machine.h"	/* for BLCKSZ */
#include "tmp/libpq-fs.h" /* for pgstat */

#ifdef PARALLELDEBUG
#include "executor/paralleldebug.h"
#endif

RcsId("$Header$");

#ifdef PORTNAME_sparc
/*
 * the SunOS 4 NOFILE is a lie, because the default limit is *not* the
 * maximum number of file descriptors you can have open.
 *
 * we have to either use this number (the default dtablesize) or explicitly
 * call setrlimit(RLIMIT_NOFILE, NOFILE).
 */
#include <sys/user.h>
#undef NOFILE
#define NOFILE NOFILE_IN_U
#endif /* PORTNAME_sparc */

#ifdef sequent
#define RESERVE_FOR_LD	5
#else
#define RESERVE_FOR_LD	10
#endif

#ifdef SONY_JUKEBOX
#define RESERVE_FOR_JB	1
#endif

#ifdef SONY_JUKEBOX
#define	MAXFILES	((NOFILE - RESERVE_FOR_LD) - RESERVE_FOR_JB)
#else /* SONY_JUKEBOX */
#define	MAXFILES	(NOFILE - RESERVE_FOR_LD)
#endif /* SONY_JUKEBOX */

/* #define FDDEBUG /* */

/* Debugging.... */


#ifdef FDDEBUG
# define DO_DB(A) A
# define private /* static */
#else
# define DO_DB(A) /* A */
# define private static
#endif

#define VFD_CLOSED -1

#include "storage/fd.h"
#include "utils/log.h"

#define FileIsNotOpen(file) (VfdCache[file].fd == VFD_CLOSED)

typedef struct vfd {
	signed short	fd;
	unsigned short	fdstate;

#define FD_DIRTY	(1 << 0)

	FileNumber	nextFree;
	FileNumber	lruMoreRecently;
	FileNumber	lruLessRecently;
	long		seekPos;
	char		*fileName;
	int		fileFlags;
	int		fileMode;
} Vfd;

/*
 *
 * Striped file descriptor struct
 *
 *
 */
int NStriping = 1;		/* degree of striping, default as 1 */
int StripingMode = 0;		/* default striping mode to RAID 0 */
typedef struct sfd {
        FileNumber vfd[NDISKS];
        int curStripe;
        long seekPos;
	long endPos;
        FileNumber nextFree;
} Sfd;

private Sfd *SfdCache;

/*
 *
 * Lru stands for Least Recently Used.
 * Vfd stands for Virtual File Descriptor
 *
 */

/*
 * Virtual File Descriptor array pointer and size
 */

private	Vfd	*VfdCache;

private	Size	SizeVfdCache = 0;
private Size	SizeSfdCache = 100;

/*
 * Minimun number of file descriptors known to be free
 */

private FreeFd = 0;

private	nfile = 0;

/*
 * delete a file from the Last Recently Used ring.
 * the ring is a doubly linked list that begins and
 * ends on element zero.
 *
 * example:
 *
 *     /--less----\                /---------\
 *     v           \              v           \
 *   #0 --more---> LeastRecentlyUsed --more-\ \
 *    ^\                                    | |
 *     \\less--> MostRecentlyUsedFile   <---/ |
 *      \more---/                    \--less--/
 *
 */
void
_dump_lru()
{
    int mru = VfdCache[0].lruLessRecently;
    Vfd *vfdP = &VfdCache[mru];

    printf("MOST %d ", mru);
    while (mru != 0)
    {
	mru = vfdP->lruLessRecently;
	vfdP = &VfdCache[mru];
    	printf("%d ", mru);
    }
    printf("LEAST\n");
}
void
EnableFDCache(enable)
int enable;
{
    static int numEnabled;
#ifdef DOUBLECHECK
    static int fault;

    Assert(!fault);
#endif

    if (!enable && --numEnabled)
	return;
    if (enable && numEnabled++)
	return;

#ifdef DOUBLECHECK
    ++fault;
#endif
    if (enable) {
	EnableELog(1);
	/* XXX */
    } else {
	EnableELog(0);
	/* XXX */
    }
#ifdef DOUBLECHECK
    --fault;
#endif
}


 /*
  * Private Routines
  *
  * Delete	   - delete a file from the Lru ring
  * LruDelete	   - remove a file from the Lru ring and close
  * Insert	   - put a file at the front of the Lru ring
  * LruInsert	   - put a file at the front of the Lru ring and open
  * AssertLruRoom  - make sure that there is a free fd.
  * AllocateVfd	   - grab a free (or new) file record (from VfdArray)
  * FreeVfd	   - free a file record
  *
  */

void
LruDelete ARGS((
	FileNumber	file
));

int
LruInsert ARGS((
	FileNumber	file
));

void
AssertLruRoom();

FileNumber
AllocateVfd();

void
Delete (file)
	FileNumber	file;
{
	Vfd	*fileP;

	DO_DB(printf("DEBUG:	Delete %d (%s)\n",file,VfdCache[file].fileName));
	DO_DB(_dump_lru());

	Assert(file != 0);

	fileP = &VfdCache[file];
	VfdCache[fileP->lruLessRecently].lruMoreRecently =
		VfdCache[file].lruMoreRecently;
	VfdCache[fileP->lruMoreRecently].lruLessRecently =
		VfdCache[file].lruLessRecently;

	DO_DB(_dump_lru());
}


void
LruDelete(file)
	FileNumber	file;
{
	Vfd     *fileP;
	int	returnValue;

	DO_DB(printf("DEBUG:	LruDelete %d (%s)\n",file,VfdCache[file].fileName));

	Assert(file != 0);

	fileP = &VfdCache[file];

	/* delete the vfd record from the LRU ring */
	Delete(file);

	/* save the seek position */
	fileP->seekPos = lseek(fileP->fd, 0L, L_INCR);
	Assert( fileP->seekPos != -1);

	/* if we have written to the file, sync it */
	if (fileP->fdstate & FD_DIRTY) {
	    returnValue = fsync(fileP->fd);
	    Assert(returnValue != -1);
	    fileP->fdstate &= ~FD_DIRTY;
	}

	/* close the file */
	returnValue = close(fileP->fd);
	Assert(returnValue != -1);

	--nfile;
	fileP->fd = VFD_CLOSED;

	/* note that there is now one more free real file descriptor */
	FreeFd++;
}

void
Insert(file)
	FileNumber	file;
{
	Vfd	*vfdP;

	DO_DB(printf("DEBUG:	Insert %d (%s)\n",file,VfdCache[file].fileName));
	DO_DB(_dump_lru());

	vfdP = &VfdCache[file];

	vfdP->lruMoreRecently = 0;
	vfdP->lruLessRecently = VfdCache[0].lruLessRecently;
	VfdCache[0].lruLessRecently = file;
	VfdCache[vfdP->lruLessRecently].lruMoreRecently = file;

	DO_DB(_dump_lru());
}

int
LruInsert (file)
	FileNumber	file;
{
	Vfd	*vfdP;
	int	returnValue;

	DO_DB(printf("DEBUG:	LruInsert %d (%s)\n",file,VfdCache[file].fileName));

	vfdP = &VfdCache[file];

	if (FileIsNotOpen(file)) {
	       int tmpfd;

		/*
		 * Open the requested file
		 *
		 * Note, due to bugs in sequent operating system and
		 * possibly other OS's, we must ensure a file descriptor
		 * is available before attempting to open a normal file
		 * if O_CREAT is specified.
		 */

tryAgain:
		tmpfd = open("/dev/null", O_CREAT|O_RDWR, 0666);
		if (tmpfd < 0) {
		    FreeFd = 0;
		    errno = 0;
		    AssertLruRoom();
		    goto tryAgain;
		} else {
		    close(tmpfd);
		}
		vfdP->fd = open(vfdP->fileName,vfdP->fileFlags,vfdP->fileMode);

		if (vfdP->fd < 0) {
		    DO_DB(printf("RE_OPEN FAILED: %d\n", errno));
		    return (vfdP->fd);
		} else {
		    DO_DB(printf("RE_OPEN SUCESS\n"));
		    ++nfile;
		}

		/* seek to the right position */
		if (vfdP->seekPos != 0L) {
			returnValue = lseek(vfdP->fd, vfdP->seekPos, L_SET);
			Assert(returnValue != -1);
		}

		/* init state on open */
		vfdP->fdstate = 0x0;

		/* note that a file descriptor has been used up */
		if (FreeFd > 0)
			FreeFd--;
	}

	/*
	 * put it at the head of the Lru ring
	 */

	Insert(file);

	return (0);
}

void
AssertLruRoom()
{
	DO_DB(printf("DEBUG:	AssertLruRoom (FreeFd = %d)\n",FreeFd));

	if (FreeFd <= 0 || nfile >= MAXFILES) {
		LruDelete(VfdCache[0].lruMoreRecently);
	}
}

int
FileAccess(file)
	FileNumber	file;
{
	int	returnValue;

	DO_DB(printf("DB: FileAccess %d (%s)\n",file,VfdCache[file].fileName));

	/*
	 * Is the file open?  If not, close the least recently used,
	 * then open it and stick it at the head of the used ring
	 */

	if (FileIsNotOpen(file)) {

		AssertLruRoom();

		returnValue = LruInsert(file);
		if (returnValue != 0)
			return returnValue;

	} else {

		/*
		 * We now know that the file is open and that it is not the
		 * last one accessed, so we need to more it to the head of
		 * the Lru ring.
		 */

		Delete(file);
		Insert(file);
	}

	return (0);
}

FileNumber
AllocateVfd()
{
	Index	i;
	FileNumber	file;

	DO_DB(printf("DEBUG:	AllocateVfd\n"));

	if (SizeVfdCache == 0) {

		/* initialize */
		VfdCache = (Vfd *)malloc(sizeof(Vfd));

		VfdCache->nextFree = 0;
		VfdCache->lruMoreRecently = 0;
		VfdCache->lruLessRecently = 0;
		VfdCache->fd = VFD_CLOSED;
		VfdCache->fdstate = 0x0;

		SizeVfdCache = 1;
	}

	if (VfdCache[0].nextFree == 0) {

		/*
		 * The free list is empty so it is time to increase the
		 * size of the array
		 */

		VfdCache =(Vfd *)realloc(VfdCache, sizeof(Vfd)*SizeVfdCache*2);
		Assert(VfdCache != NULL);

		/*
		 * Set up the free list for the new entries
		 */

		for (i = SizeVfdCache; i < 2*SizeVfdCache; i++)  {
			bzero((char *) &(VfdCache[i]), sizeof(VfdCache[0]));
			VfdCache[i].nextFree = i+1;
			VfdCache[i].fd = VFD_CLOSED;
		    }

		/*
		 * Element 0 is the first and last element of the free
		 * list
		 */

		VfdCache[0].nextFree = SizeVfdCache;
		VfdCache[2*SizeVfdCache-1].nextFree = 0;

		/*
		 * Record the new size
		 */

		SizeVfdCache *= 2;
	}
	file = VfdCache[0].nextFree;

	VfdCache[0].nextFree = VfdCache[file].nextFree;

    	return file;
}

/*
 *  Called when we get a shared invalidation message on some relation.
 */
void
FileInvalidate(file)
    File file;
{
    Sfd *sfdP;
    int i;
    int n;

    /* avoid work if we can */
    if (file < 0)
	return;

    sfdP = &SfdCache[file];

    n = NStriping;
    if (StripingMode == 1) n *= 2;
    for (i=0; i<n; i++)
    {
	if (!FileIsNotOpen(sfdP->vfd[i]))
	    LruDelete(sfdP->vfd[i]);
    }
}

void
FreeVfd(file)
	FileNumber	file;
{
	DO_DB(printf("DB: FreeVfd: %d (%s)\n",file,VfdCache[file].fileName));

	VfdCache[file].nextFree = VfdCache[0].nextFree;
	VfdCache[0].nextFree = file;
}

/* VARARGS2 */
FileNumber
fileNameOpenFile(fileName, fileFlags, fileMode)
	FileName	fileName;
	int		fileFlags;
	int		fileMode;
{
	static int osRanOut = 0;
	FileNumber	file;
	Vfd	*vfdP;
	int     tmpfd;

	DO_DB(printf("DEBUG: FileNameOpenFile: %s %x %o\n",fileName,fileFlags,fileMode));

	file = AllocateVfd();
	vfdP = &VfdCache[file];

	if (nfile >= MAXFILES || (FreeFd == 0 && osRanOut)) {
		AssertLruRoom();
	}

tryAgain:
	tmpfd = open("/dev/null", O_CREAT|O_RDWR, 0666);
	if (tmpfd < 0) {
	    DO_DB(printf("DB: not enough descs, retry, er= %d\n", errno));
	    errno = 0;
	    FreeFd = 0;
	    osRanOut = 1;
	    AssertLruRoom();
	    goto tryAgain;
	} else {
	    close(tmpfd);
	}

	vfdP->fd = open(fileName,fileFlags,fileMode);
	vfdP->fdstate = 0x0;

	if (vfdP->fd < 0) {
	    FreeVfd(file);
	    return -1;
	}
	++nfile;
	DO_DB(printf("DB: FNOF success %d\n", vfdP->fd));

	(void)LruInsert(file);

	if (fileName==NULL) {
	    elog(WARN, "fileNameOpenFile: NULL fname");
	}
	vfdP->fileName = malloc(strlen(fileName)+1);
	strcpy(vfdP->fileName,fileName);

	vfdP->fileFlags = fileFlags & ~(O_TRUNC|O_EXCL);
	vfdP->fileMode = fileMode;

	return file;
}

void
fileClose(file)
	FileNumber	file;
{
	int	returnValue;

	DO_DB(printf("DEBUG: FileClose: %d (%s)\n",file,VfdCache[file].fileName));

	if (!FileIsNotOpen(file)) {

		/* remove the file from the lru ring */
		Delete(file);

		/* record the new free operating system file descriptor */
		FreeFd++;

		/* if we did any writes, sync the file before closing */
		if (VfdCache[file].fdstate & FD_DIRTY) {
		    returnValue = fsync(VfdCache[file].fd);
		    Assert(returnValue != -1);
		    VfdCache[file].fdstate &= ~FD_DIRTY;
		}

		/* close the file */
		returnValue = close(VfdCache[file].fd);
		Assert(returnValue != -1);

		--nfile;
		VfdCache[file].fd = VFD_CLOSED;
	}
	/*
	 * Add the Vfd slot to the free list
	 */
	FreeVfd(file);
	/*
	 * Free the filename string
	 */
	free(VfdCache[file].fileName);

	return;
}

void
fileUnlink(file)
	FileNumber	file;
{
	int returnValue;

	DO_DB(printf("DB: FileClose: %d (%s)\n",file,VfdCache[file].fileName));

	if (!FileIsNotOpen(file)) {

		/* remove the file from the lru ring */
		Delete(file);

		/* record the new free operating system file descriptor */
		FreeFd++;

		/* if we did any writes, sync the file before closing */
		if (VfdCache[file].fdstate & FD_DIRTY) {
		    returnValue = fsync(VfdCache[file].fd);
		    Assert(returnValue != -1);
		    VfdCache[file].fdstate &= ~FD_DIRTY;
		}

		/* close the file */
		returnValue = close(VfdCache[file].fd);
		Assert(returnValue != -1);

		--nfile;
		VfdCache[file].fd = VFD_CLOSED;
	}
	/* add the Vfd slot to the free list */
	FreeVfd(file);

	/* free the filename string */
	unlink(VfdCache[file].fileName);
	free(VfdCache[file].fileName);

	return;
}

Amount
fileRead (file, buffer, amount)
	FileNumber	file;
	String	buffer;
	Amount	amount;
{
	int	returnCode;
	DO_DB(printf("DEBUG: FileRead: %d (%s) %d 0x%x\n",file,VfdCache[file].fileName,amount,buffer));

	FileAccess(file);
	returnCode = read(VfdCache[file].fd, buffer, amount);

	return returnCode;
}


Amount
fileWrite (file, buffer, amount)
	FileNumber	file;
	String	buffer;
	Amount	amount;
{
	int	returnCode;
	DO_DB(printf("DB: FileWrite: %d (%s) %d 0x%lx\n",file,VfdCache[file].fileName,amount,buffer));

	FileAccess(file);
	returnCode = write(VfdCache[file].fd, buffer, amount);

	if (returnCode > 0) {  /* changed by Boris with Mao's advice */
	    VfdCache[file].seekPos += returnCode;
	}
    
	/* record the write */
	VfdCache[file].fdstate |= FD_DIRTY;

	return returnCode;
}

long
fileSeek (file, offset, whence)
	FileNumber	file;
	long	offset;
	int	whence;
{
	int	returnCode;

	DO_DB(printf("DEBUG: FileSeek: %d (%s) %d %d\n",file,VfdCache[file].fileName,offset,whence));

	if (FileIsNotOpen(file)) {

		switch(whence) {
		  case L_SET:
			VfdCache[file].seekPos = offset;
			return offset;

		  case L_INCR:
			VfdCache[file].seekPos = VfdCache[file].seekPos +offset;
			return VfdCache[file].seekPos;

		  case L_XTND:
			FileAccess(file);
			returnCode = VfdCache[file].seekPos = 
				lseek(VfdCache[file].fd, offset, whence);
			return returnCode;

		  default:
			elog(WARN,"should not be here in FileSeek %d", whence);
			break;
		}
	} else {
		returnCode = VfdCache[file].seekPos = 
			lseek(VfdCache[file].fd, offset, whence);
		return returnCode;
	}

	elog(WARN,"should not be here in FileSeek #2");
	return 0L;
}

long
fileTell (file)
	FileNumber	file;
{
	DO_DB(printf("DEBUG: FileTell %d (%s)\n",file,VfdCache[file].fileName));
	return VfdCache[file].seekPos;
}

int
fileSync (file)
	FileNumber	file;
{
	int	returnCode;

	/*
	 *  If the file isn't open, then we don't need to sync it; we always
	 *  sync files when we close them.  Also, if we haven't done any
	 *  writes that we haven't already synced, we can ignore the request.
	 */

	if (VfdCache[file].fd < 0 || !(VfdCache[file].fdstate & FD_DIRTY)) {
	    returnCode = 0;
	} else {
	    returnCode = fsync(VfdCache[file].fd);
	    VfdCache[file].fdstate &= ~FD_DIRTY;
	}

	return returnCode;
}

/*
 * keep track of how many have been allocated....   give a
 * warning if there are too few left
 */

int allocatedFiles = 0;

/*
 * Note:
 *	This is expected to return on failure by AllocateFiles().
 */
void
AllocateFile()
{
	int fd;

	while ((fd = open("/dev/null",O_WRONLY,0)) < 0) {
		if (errno == EMFILE) {
			errno = 0;
			FreeFd = 0;
			AssertLruRoom();
		} else {
			elog(WARN,"Open: %s in %s line %d\n", "/dev/null",
				__FILE__, __LINE__);
		}
	}
	close(fd);
	if (MAXFILES - ++allocatedFiles < 6)
		elog(DEBUG,"warning: few useable file descriptors left (%d)",
			MAXFILES - allocatedFiles);

	DO_DB(printf("DEBUG: AllocatedFile.  FreeFd = %d\n",FreeFd));
}

uint16
AllocateFiles(fileCount)
	uint16	fileCount;
{
	int	fd;

	DO_DB(printf("DEBUG: Allocate Files: %d\n",fileCount));

	if (fileCount == 0) {
		return (0);
	}

	AllocateFile();

	fd = open("/dev/null", O_WRONLY, 0);
	if (fd == -1) {
		return (0);
	} else {
		uint16	openedFileCount =
			1 + AllocateFiles(fileCount - 1);

		close(fd);

		nfile -= openedFileCount;
		return (openedFileCount);
	}
}

/*
 * What happens if FreeFile() is called without a previous AllocateFile()?
 */
void
FreeFile()
{
	DO_DB(printf("DEBUG: FreeFile.  FreeFd now %d\n",FreeFd));
	FreeFd++;
	nfile++;			/* dangerous */
	Assert(allocatedFiles > 0);
	--allocatedFiles;
}

/*
 * What happens if FreeFiles(N) is called without a previous AllocateFile(N)?
 */
void
FreeFiles(fileCount)
	int	fileCount;
{
	if (fileCount >= 0) {
		FreeFd += fileCount;
		nfile += fileCount;	/* XXX dangerous */
	}
	Assert(allocatedFiles >= fileCount);
	allocatedFiles -= fileCount;
	DO_DB(printf("DEBUG: FreeFiles %d, FreeFd now %d\n",fileCount,FreeFd));
}

static long
FileSize(sfdP)
Sfd *sfdP;
{   int l=0, h=NStriping-1, m, nf;
    long lsize, hsize, msize;
    long size;
    long current_pos[10];
    
    /*  Fixed code so FileSize no longer puts the seekpos at the end of file
	now it puts it back to where it was origanally -- B.L. */

    if (h == 0) {
	current_pos[0] = fileTell(sfdP->vfd[0]);
	size = fileSeek(sfdP->vfd[0], 0l, L_XTND);
	fileSeek(sfdP->vfd[0], current_pos[0], L_SET);
	return size;
      }

    current_pos[1] = fileTell(sfdP->vfd[1]);
    current_pos[h] = fileTell(sfdP->vfd[h]);

    if ((lsize = fileSeek(sfdP->vfd[l], 0l, L_XTND)) < 0) {
	elog(FATAL, "lseek:%m");
    }
    fileSeek(sfdP->vfd[1], current_pos[1], L_SET);
    if ((hsize = fileSeek(sfdP->vfd[h], 0l, L_XTND)) < 0) {
	elog(FATAL, "lseek:%m");
    }
    fileSeek(sfdP->vfd[h], current_pos[h], L_SET);
    if (lsize == hsize)
       nf = 0;
    else {
	while (l + 1 != h) {
	    m = (l + h) / 2;
	    current_pos[m] = fileTell(sfdP->vfd[m]);
	if ((msize = fileSeek(sfdP->vfd[m], 0l, L_XTND)) < 0) {
	    elog(FATAL, "lseek:%m");
	    fileSeek(sfdP->vfd[m], current_pos[m], L_SET);
	}
	    if (msize > hsize)
	       l = m;
	    else
	       h = m;
	  }
	nf = h;
      }
    size = hsize * NStriping + nf * BLCKSZ;
    return size;
}

File
FileNameOpenFile(fileName, fileFlags, fileMode)
FileName fileName;
int fileFlags;
int fileMode;
{
    int i;
    int n;
    File sfd;
    Sfd *sfdP;
    char *fname, *filepath();
    if (SfdCache == NULL) {
       SfdCache = (Sfd*)malloc(SizeSfdCache * sizeof(Sfd));
       for (i=0; i<SizeSfdCache-1; i++)
	 SfdCache[i].nextFree = i + 1;
       SfdCache[SizeSfdCache - 1].nextFree = 0;
    }
    sfd = SfdCache[0].nextFree;
    if (sfd == 0)  {
       SfdCache = (Sfd*)realloc(SfdCache, sizeof(Sfd)*SizeSfdCache*2);
       Assert(SfdCache != NULL);
       for (i = SizeSfdCache; i < 2*SizeSfdCache; i++) {
	   bzero((char *) &(SfdCache[i]), sizeof(SfdCache[0]));
	   SfdCache[i].nextFree = i + 1;
       }
       SfdCache[0].nextFree = SizeSfdCache;
       SfdCache[2*SizeSfdCache-1].nextFree = 0;
       SizeSfdCache *= 2;
       sfd = SfdCache[0].nextFree;
      }
    SfdCache[0].nextFree = SfdCache[sfd].nextFree;
    sfdP = &(SfdCache[sfd]);
    n = NStriping;
    if (StripingMode == 1) n *= 2;
    for (i=0; i<n; i++) {
	fname = filepath(fileName, i);
	if ((sfdP->vfd[i] = fileNameOpenFile(fname, fileFlags, fileMode)) < 0)
	    return ((File) sfdP->vfd[i]);
    }
    sfdP->curStripe = 0;
    sfdP->seekPos = 0;
    sfdP->endPos = FileSize(sfdP);
    return(sfd);
}

File
PathNameOpenFile(fileName, fileFlags, fileMode)
FileName fileName;
int fileFlags;
int fileMode;
{
    int i;
    int n;
    File sfd;
    Sfd *sfdP;
    char *fname, *filepath();
    if (SfdCache == NULL) {
       SfdCache = (Sfd*)malloc(SizeSfdCache * sizeof(Sfd));
       for (i=0; i<SizeSfdCache-1; i++)
	 SfdCache[i].nextFree = i + 1;
       SfdCache[SizeSfdCache - 1].nextFree = 0;
    }
    sfd = SfdCache[0].nextFree;
    if (sfd == 0)  {
       SfdCache = (Sfd*)realloc(SfdCache, sizeof(Sfd)*SizeSfdCache*2);
       Assert(SfdCache != NULL);
       for (i = SizeSfdCache; i < 2*SizeSfdCache; i++) {
	   bzero((char *) &(SfdCache[i]), sizeof(SfdCache[0]));
	   SfdCache[i].nextFree = i + 1;
       }
       SfdCache[0].nextFree = SizeSfdCache;
       SfdCache[2*SizeSfdCache-1].nextFree = 0;
       SizeSfdCache *= 2;
       sfd = SfdCache[0].nextFree;
      }
    SfdCache[0].nextFree = SfdCache[sfd].nextFree;
    sfdP = &(SfdCache[sfd]);
    n = NStriping;
    if (StripingMode == 1) n *= 2;
    for (i=0; i<n; i++) {
	if ((sfdP->vfd[i] = fileNameOpenFile(fileName, fileFlags, fileMode)) < 0)
	    return ((File) sfdP->vfd[i]);
    }
    sfdP->curStripe = 0;
    sfdP->seekPos = 0;
    sfdP->endPos = FileSize(sfdP);
    return(sfd);
}

void
FileClose(file)
File file;
{
    int i;
    int n = NStriping;
    Sfd *sfdP;

    sfdP = &(SfdCache[file]);
    if (StripingMode == 1) n *= 2;
    for (i=0; i<n; i++)
	fileClose(sfdP->vfd[i]);
    sfdP->nextFree = SfdCache[0].nextFree;
    SfdCache[0].nextFree = file;
}

extern int MyPid;

Amount
FileRead(file, buffer, amount)
File file;
String buffer;
Amount amount;
{
   Sfd *sfdP;
   Amount ret;
#ifdef PARALLELDEBUG
   BeginParallelDebugInfo(PDI_FILEREAD);
#endif
   sfdP = &(SfdCache[file]);
   switch (StripingMode) {
   case 0:
       ret = fileRead(sfdP->vfd[sfdP->curStripe], buffer, amount);
       sfdP->curStripe = (sfdP->curStripe + 1) % NStriping;
       break;
   case 1:
       if (MyPid % 2 == 0)
           ret = fileRead(sfdP->vfd[sfdP->curStripe], buffer, amount);
       else
           ret = fileRead(sfdP->vfd[sfdP->curStripe+NStriping], buffer, amount);
       break;
    case 5:
    /*
       printf("READ %s stripe %d row %d amount %d\n", VfdCache[sfdP->vfd[0]].fileName, sfdP->curStripe, VfdCache[sfdP->vfd[sfdP->curStripe]].seekPos/BLCKSZ, amount);
       */
       ret = fileRead(sfdP->vfd[sfdP->curStripe], buffer, amount);
       break;
      }
#ifdef PARALLELDEBUG
   EndParallelDebugInfo(PDI_FILEREAD);
#endif
   return(ret);
}

#define XOR(X, Y) (~(X) & (Y) | (X) & ~(Y))
static char *
blkxor(blk1, blk2, blk3, resblk)
char *blk1, *blk2, *blk3, *resblk;
{
    int i;
    char c1, c2, c3;
    char t;
    for (i=0; i<BLCKSZ; i++) {
	c1 = (blk1 == NULL)?0:blk1[i];
	c2 = (blk2 == NULL)?0:blk2[i];
	c3 = (blk3 == NULL)?0:blk3[i];
	t = XOR(c1, c2);
	resblk[i] = XOR(t, c3);
      }
    return resblk;
}

Amount
FileWrite(file, buffer, amount)
File file;
String buffer;
Amount amount;
{
    Sfd *sfdP;
    Amount ret;
    bool extend;

#ifdef PARALLELDEBUG
   BeginParallelDebugInfo(PDI_FILEWRITE);
#endif
    sfdP = &(SfdCache[file]);
    /*
    printf("WRITE %s offset %d amount %d\n", VfdCache[sfdP->vfd[0]].fileName, sfdP->seekPos, amount);
    */
    switch (StripingMode) {
    case 0:
        if (sfdP->seekPos >= sfdP->endPos)
	    sfdP->endPos = sfdP->seekPos + amount;
        ret = fileWrite(sfdP->vfd[sfdP->curStripe], buffer, amount);
	if (ret > 0) /* added by Boris with Mao's advice */
	    sfdP->seekPos += ret;
        sfdP->curStripe = (sfdP->curStripe + 1) % NStriping;
	break;
    case 1:
        if (sfdP->seekPos >= sfdP->endPos)
	    sfdP->endPos = sfdP->seekPos + amount;
        ret = fileWrite(sfdP->vfd[sfdP->curStripe], buffer, amount);
        ret = fileWrite(sfdP->vfd[sfdP->curStripe+NStriping], buffer, amount);
	if (ret > 0) /* Added by Boris with Mao's advice */
	    sfdP->seekPos += ret;
	break;
    case 5:
	{
            char oldblk[BLCKSZ], oldparblk[BLCKSZ], newparblk[BLCKSZ];
	    char *parblk = NULL;
	    int blknum, rownum, parstripe;
	    long parPos;

	    blknum = sfdP->seekPos/BLCKSZ;
	    rownum = blknum/NStriping;
	    parstripe = NStriping - 1 - rownum % NStriping;
	    if ((sfdP->seekPos >= sfdP->endPos) && 
		(sfdP->curStripe == 0 ||
		 (parstripe == 0 && sfdP->curStripe == 1))) {
	        parblk = buffer;
		if (parstripe == 0)
		    sfdP->endPos = sfdP->seekPos + BLCKSZ;
		else {
		    parstripe = 1;
		    sfdP->endPos = sfdP->seekPos + 2 * BLCKSZ;
		  }
	      }
	    else {
	        parPos = (rownum * NStriping + parstripe) * BLCKSZ;
	        if (parPos >= sfdP->endPos) {
		    parPos = sfdP->endPos - BLCKSZ;
		    parstripe = (parPos/BLCKSZ) % NStriping;
	          }
		ret = fileSeek(sfdP->vfd[parstripe],(long)rownum*BLCKSZ,L_SET);
		/*
		printf("read old parity at stripe %d row %d\n", parstripe, VfdCache[sfdP->vfd[parstripe]].seekPos/BLCKSZ);
		*/
		ret = fileRead(sfdP->vfd[parstripe], oldparblk, BLCKSZ);
		if (sfdP->seekPos >= sfdP->endPos ||
		    sfdP->seekPos == parPos) {
		    parblk = blkxor(oldparblk, NULL, buffer, newparblk);
		  }
		else {
		/*
		    printf("read old block at stripe %d row %d\n", sfdP->curStripe, VfdCache[sfdP->vfd[sfdP->curStripe]].seekPos/BLCKSZ);
		    */
	            ret = fileRead(sfdP->vfd[sfdP->curStripe], oldblk, BLCKSZ);
		    parblk = blkxor(oldparblk, oldblk, buffer, newparblk);
		  }
	        if (sfdP->seekPos >= sfdP->endPos) {
		    sfdP->endPos = sfdP->seekPos + BLCKSZ;
	          }
		else if (sfdP->seekPos == parPos) {
		    parstripe++;
		    sfdP->endPos += BLCKSZ;
		  }
	      }
	    ret = fileSeek(sfdP->vfd[parstripe],(long)rownum*BLCKSZ,L_SET);
	    /*
	    printf("write parity at stripe %d row %d\n", parstripe, VfdCache[sfdP->vfd[parstripe]].seekPos/BLCKSZ);
	    */
	    ret = fileWrite(sfdP->vfd[parstripe], parblk, BLCKSZ);
	    ret =fileSeek(sfdP->vfd[sfdP->curStripe],(long)rownum*BLCKSZ,L_SET);
	    /*
	    printf("write new block at stripe %d row %d\n", sfdP->curStripe, VfdCache[sfdP->vfd[sfdP->curStripe]].seekPos/BLCKSZ);
	    */
            ret = fileWrite(sfdP->vfd[sfdP->curStripe], buffer, amount);
	    if (ret > 0) /* added by Boris with mao's advice */
		sfdP->seekPos += ret;
	 }
       }
#ifdef PARALLELDEBUG
   EndParallelDebugInfo(PDI_FILEWRITE);
#endif
    return(ret);
}

long
FileSeek(file, offset, whence)
File file;
long offset;
int whence;
{
    int blknum;
    long blkoff;
    BlockNumber rownum;
    int nf, endstripe, parstripe;
    Sfd *sfdP;
    int ret;
#ifdef PARALLELDEBUG
    BeginParallelDebugInfo(PDI_FILESEEK);
#endif

    sfdP = &(SfdCache[file]);
    /*
    printf("SEEK %s to offset %d whence %d\n", VfdCache[sfdP->vfd[0]].fileName, offset, whence);
    */
    switch(whence) {
    case L_SET:
	switch (StripingMode) {
	case 0:
	case 1:
	    sfdP->seekPos = offset;
	    blknum = offset / BLCKSZ;
	    blkoff = offset % BLCKSZ;
	    rownum = blknum / NStriping;
	    sfdP->curStripe = nf = blknum % NStriping;
	    fileSeek(sfdP->vfd[nf], rownum * BLCKSZ + blkoff, whence);
	    if (StripingMode == 1)
		fileSeek(sfdP->vfd[nf+NStriping],rownum*BLCKSZ+blkoff,whence);
	    break;
	case 5:
	    blknum = offset / BLCKSZ;
	    blkoff = offset % BLCKSZ;
	    rownum = blknum / (NStriping - 1);
	    nf = blknum % (NStriping - 1);
	    parstripe = NStriping - 1 - rownum % NStriping;
	    if (nf >= parstripe) nf++;
	    sfdP->curStripe = nf;
	    fileSeek(sfdP->vfd[nf],rownum*BLCKSZ+blkoff,whence);
	    /*
	    printf("seek stripe %d to row %d\n", nf, rownum);
	    */
	    sfdP->seekPos = (rownum * NStriping + nf) * BLCKSZ + blkoff;
	    break;
	  }
#ifdef PARALLELDEBUG
	EndParallelDebugInfo(PDI_FILESEEK);
#endif
	return offset;
    case L_INCR:
	sfdP->seekPos = FileSeek(file, sfdP->seekPos + offset, L_SET);
#ifdef PARALLELDEBUG
	EndParallelDebugInfo(PDI_FILESEEK);
#endif
	return sfdP->seekPos;
    case L_XTND:
	switch (StripingMode) {
	case 0:
	case 1:
            blknum = sfdP->endPos / BLCKSZ;
            sfdP->curStripe = nf = blknum % NStriping;
	    offset = fileSeek(sfdP->vfd[nf], 0L, L_XTND);
	    sfdP->seekPos = sfdP->endPos = offset;
	    if (StripingMode == 1) {
		fileSeek(sfdP->vfd[nf+NStriping], 0L, L_XTND);
	      }
	    break;
	case 5:
	    sfdP->seekPos = sfdP->endPos;
	    blknum = sfdP->endPos / BLCKSZ;
	    rownum = blknum / NStriping;
	    endstripe = blknum % NStriping;
	    if (endstripe == 0)
		offset = rownum * (NStriping - 1) * BLCKSZ;
	    else
	        offset = (rownum * (NStriping - 1) + endstripe - 1) * BLCKSZ;
	    parstripe = NStriping - 1 - rownum % NStriping;
	    if (endstripe > 0 && parstripe >= endstripe) {
		sfdP->seekPos -= BLCKSZ;
		sfdP->curStripe = endstripe - 1;
		fileSeek(sfdP->vfd[endstripe - 1], rownum*BLCKSZ, L_SET);
		/*
		printf("seek stripe %d to row %d\n", endstripe-1, rownum);
		*/
	      }
	    else {
		if (endstripe == parstripe && endstripe == 0) {
		    sfdP->seekPos += BLCKSZ;
		    endstripe++;
		  }
		sfdP->curStripe = endstripe;
	        ret = fileSeek(sfdP->vfd[endstripe], 0L, L_XTND);
		/*
		printf("seek stripe %d to row %d\n", endstripe, ret/BLCKSZ);
		*/
	      }
	    break;
	  }
#ifdef PARALLELDEBUG
	EndParallelDebugInfo(PDI_FILESEEK);
#endif
	return offset;
      }

    /*
     * probably never gets here, but to keep lint happy...
     */

    return(0);
}

long
FileTell(file)
File file;
{
   return SfdCache[file].seekPos;
}

int
FileSync(file)
File file;
{
    int i, returnCode;
    Sfd *sfdP;
    sfdP = &(SfdCache[file]);
    for (i=0; i<NStriping; i++)
       returnCode = fileSync(sfdP->vfd[i]);
    return returnCode;
}

char *PostgresHomes[NDISKS];
char *DBName;
extern char *DataDir;

char *
filepath(filename, stripe)
char *filename;
int stripe;
{
    char *buf;
    int len;

    if (*filename != '/') {
	len = strlen(DataDir) + strlen("/base/") + strlen(DBName)
	      + strlen(filename) + 2;
	buf = (char*) palloc(len);
	sprintf(buf, "%s/base/%s/%s", DataDir, DBName, filename);
    } else {
	buf = (char *) palloc(strlen(filename) + 1);
	strcpy(buf, filename);
    }

    return(buf);
}

int
FileNameUnlink(filename)
char *filename;
{
    int i, returnCode = 0;
    for (i=0; i<NStriping; i++)
       if (unlink(filepath(filename, i)) < 0)
	  returnCode = -1;
    return returnCode;
}

BlockNumber
FileGetNumberOfBlocks(file)
File file;
{
    long len;
    BlockNumber nblks;

    len = FileSeek(file, 0L, L_XTND) - 1;
    return((BlockNumber)((len < 0) ? 0 : 1 + len / BLCKSZ));
}

void
FileUnlink(file)
File file;
{
    int i;
    int n;
    Sfd *sfdP;

    sfdP = &(SfdCache[file]);

    n = NStriping;
    if (StripingMode == 1) n *= 2;
    for (i=0; i<n; i++)
       fileUnlink(sfdP->vfd[i]);
}

/*--------------------------------------------------------
 *
 * FileFindName
 *
 * Return the name of the given file (used for debugging).
 *
 *--------------------------------------------------------
 */

char *
FileFindName(file)
File file;
{
    Sfd *sfdP;
    char *ret;
    char *fileFindName();
    char *s; int i;

    ret = VfdCache[file].fileName;

    if (ret==NULL) {
	return("<null>");
    }
    /*
     * strip the path name
     */
    i = 0;
    while (ret[i] != '\0') {
	if (ret[i] == '/') {
	    s = &(ret[i+1]);
	}
	i++;
    }

    return(s);

}

void
closeAllVfds()
{
    int i;
    for (i=0; i<SizeVfdCache; i++) {
	if (!FileIsNotOpen(i))
	    LruDelete(i);
      }
}

int
OurSystem(cmd)
char *cmd;
{
#ifdef sequent
    closeAllVfds();
#endif
    return system(cmd);
}

void
closeOneVfd()
{
    int tmpfd;

    tmpfd = open("/dev/null", O_CREAT | O_RDWR, 0666);
    if (tmpfd < 0) {
       FreeFd = 0;
       AssertLruRoom();
       FreeFd = 0;
     }
    else
       close(tmpfd);
}

int FileStat(file,stbuf)
     File file;
     struct pgstat *stbuf;
{
    int ret;
    struct stat ustatbuf;
    ret = fstat(VfdCache[SfdCache[file].vfd[0]].fd,&ustatbuf);
    if (ret >= 0) {
	stbuf->st_mode = ustatbuf.st_mode;
	stbuf->st_uid = ustatbuf.st_uid;
	stbuf->st_size = ustatbuf.st_size;
	stbuf->st_sizehigh = 0;
#if defined(PORTNAME_bsd44)
	/* 
	 * XXX truely bogus move on 44's part - st_?time are now #defined.
	 * So we do the expansion.
	 */
	stbuf->st_atime = ustatbuf.st_atimespec.ts_sec;
	stbuf->st_ctime = ustatbuf.st_ctimespec.ts_sec;
	stbuf->st_mtime = ustatbuf.st_mtimespec.ts_sec;
#else
	stbuf->st_atime = ustatbuf.st_atime;
	stbuf->st_ctime = ustatbuf.st_ctime;
	stbuf->st_mtime = ustatbuf.st_mtime;
#endif
    }
    return ret;
}
