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
 *
 * NOTE: SEQUENT OS BUG: on open w/ O_CREAT if EMFILE error occurs the
 * file is *still* created in the directory.  Therefore, we must ensure
 * fd descriptors are available before calling open if O_CREAT flag is
 * set.
 */

#ifdef sequent
#define RESERVE_FOR_LD	5
#else
#define RESERVE_FOR_LD	10
#endif

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

extern errno;

#define	MAXFILES	(NOFILE-RESERVE_FOR_LD)

#include "tmp/c.h"
#include "machine.h"	/* for BLCKSZ */

#ifdef PARALLELDEBUG
#include <usclkc.h>
int FileReadCount;
usclk_t FileReadTime;
#endif

RcsId("$Header$");

/* #define FDDEBUG /* */

/* Debugging.... */


#ifdef FDDEBUG
# define DO_DB(A) A
# define private /* static */
#else
# define DO_DB(A) /* A */
# define private static
#endif

#define FileClosed -1

#include "storage/fd.h"
#include "utils/log.h"

#define FileIsNotOpen(file) (VfdCache[file].fd == FileClosed)

typedef struct vfd {
	signed short	fd;
	FileNumber	nextFree;
	FileNumber	lruMoreRecently;
	FileNumber	lruLessRecently;
	long	seekPos;
	char	*fileName;
	int	fileFlags;
	int	fileMode;
} Vfd;

/*
 *
 * Striped file descriptor struct
 *
 *
 */
int NStriping = 1;		/* degree of striping, default as 1 */
typedef struct sfd {
        FileNumber vfd[NDISKS];
        int curStripe;
        long seekPos;
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

/*
 * Minimun number of file descriptors known to be free
 */

FreeFd = 0;

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

private
void
LruDelete ARGS((
	FileNumber	file
));

private
int
LruInsert ARGS((
	FileNumber	file
));

void
AssertLruRoom();

private FileNumber
AllocateVfd();

private inline void
Delete (file)
	FileNumber	file;
{
	Vfd	*fileP;
	
	DO_DB(printf("DEBUG:	Delete %d (%s)\n",file,VfdCache[file].fileName));

	Assert(file != 0);

	fileP = &VfdCache[file];
	VfdCache[fileP->lruLessRecently].lruMoreRecently =
		VfdCache[file].lruMoreRecently;
	VfdCache[fileP->lruMoreRecently].lruLessRecently =
		VfdCache[file].lruLessRecently;
}


private inline void
LruDelete(file)
	FileNumber	file;
{
	Vfd     *fileP;
	int	returnValue;

	DO_DB(printf("DEBUG:	LruDelete %d (%s)\n",file,VfdCache[file].fileName));

	Assert(file != 0);

	fileP = &VfdCache[file];

	/*
	 * delete the vfd record from the LRU ring
	 */

	Delete(file);

	/*
	 * save the seek position
	 */

	fileP->seekPos = lseek(fileP->fd, 0L, L_INCR);
	Assert( fileP->seekPos != -1);

	/*
	 * close the file
	 */

	returnValue = close(fileP->fd);
	Assert(returnValue != -1);
	--nfile;
	fileP->fd = FileClosed;

	/*
	 * note that there is now one more free real file descriptor
	 */

	FreeFd++;
}

private inline void
Insert(file)
	FileNumber	file;
{
	Vfd	*vfdP;
	
	DO_DB(printf("DEBUG:	Insert %d (%s)\n",file,VfdCache[file].fileName));

	vfdP = &VfdCache[file];

	vfdP->lruMoreRecently = 0;
	vfdP->lruLessRecently = VfdCache[0].lruLessRecently;
	VfdCache[0].lruLessRecently = file;
	VfdCache[vfdP->lruLessRecently].lruMoreRecently = file;
}

private	int
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

		/*
		 * Seek to the right position
		 */

		if (vfdP->seekPos != 0L) {
			returnValue = lseek(vfdP->fd, vfdP->seekPos, L_SET);
			Assert(returnValue != -1);
		}

		/*
		 * note that a file descriptor has been used up.
		 */

		if (FreeFd >0) FreeFd--;
	}

	/*
	 * put it at the head of the Lru ring
	 */

	Insert(file);

	return 0;
}

inline void
AssertLruRoom()
{
	DO_DB(printf("DEBUG:	AssertLruRoom (FreeFd = %d)\n",FreeFd));

	if (FreeFd <= 0 || nfile >= MAXFILES) {
		LruDelete(VfdCache[0].lruMoreRecently);
	}
}

private int
FileAccess(file)
	FileNumber	file;
{
	int	returnValue;
	Vfd	*vfdP;

	DO_DB(printf("DEBUG:	FileAccess  %d (%s)\n",file,VfdCache[file].fileName));

	/*
	 * Is the file open?  If not, close the least recently used
	 * then open it and stick it at the head of the used ring
	 */

	if (FileIsNotOpen(file)) {

		AssertLruRoom();

		returnValue = LruInsert(file);
		if (returnValue != 0) return returnValue;

	/* 
	 * We now know that the file is open and that is is not that last
	 * one accessed, so we need to more it to the head of the Lru ring.
	 */

	} else {
		vfdP = &VfdCache[file];

		Delete(file);

		Insert(file);
	}

	return 0;
}

private	FileNumber
AllocateVfd()
{
	Index	i;
	FileNumber	file;
	
	DO_DB(printf("DEBUG:	AllocateVfd\n"));

	if (SizeVfdCache == 0) {

		/*
		 * initialize 
		 */

		VfdCache = (Vfd *)malloc(sizeof(Vfd));

		VfdCache->nextFree = 0;
		VfdCache->lruMoreRecently = 0;
		VfdCache->lruLessRecently = 0;
		VfdCache->fd = FileClosed;

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

		for (i = SizeVfdCache; i < 2*SizeVfdCache; i++) 
			VfdCache[i].nextFree = i+1;

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
		
private inline void
FreeVfd(file)
	FileNumber	file;
{
	DO_DB(printf("DEBUG:	FreeVfd: %d (%s)\n",file,VfdCache[file].fileName));

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

		/*
		 * Remove the file from the Lru ring
		 */
		Delete(file);
		/*
		 * Note that there is now one more free operating system
		 * file descriptor available
		 */
		FreeFd++;
		/*
		 * Close the file
		 */
		returnValue = close (VfdCache[file].fd);
		Assert(returnValue != -1);
		--nfile;
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

	DO_DB(printf("DEBUG: FileClose: %d (%s)\n",file,VfdCache[file].fileName));

	if (!FileIsNotOpen(file)) {

		/*
		 * Remove the file from the Lru ring
		 */
		Delete(file);
		/*
		 * Note that there is now one more free operating system
		 * file descriptor available
		 */
		FreeFd++;
		/*
		 * Close the file
		 */
		close(VfdCache[file].fd);
		--nfile;
	}
	/*
	 * Add the Vfd slot to the free list
	 */
	FreeVfd(file);
	/*
	 * Free the filename string
	 */
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
	DO_DB(printf("DEBUG: FileWrite: %d (%s) %d 0x%x\n",file,VfdCache[file].fileName,amount,buffer));

	FileAccess(file);
	returnCode = write(VfdCache[file].fd, buffer, amount);

	return returnCode;
}


long
fileSeek (file, offset, whence)
	FileNumber	file;
	long	offset;
	int	whence;
{
	DO_DB(printf("DEBUG: FileSeek: %d (%s) %d %d\n",file,VfdCache[file].fileName,offset,whence));

	if (FileIsNotOpen(file)) {

		switch(whence) {
		case L_SET:
			VfdCache[file].seekPos = offset;
			return offset;
		case L_INCR:
			VfdCache[file].seekPos = VfdCache[file].seekPos +offset;
			return VfdCache[file].seekPos;
		case L_XTND: {
				int	returnCode;
				FileAccess(file);
				returnCode = lseek(VfdCache[file].fd, offset, whence);
				return returnCode;
			}
		default:
			elog(WARN,"should not be here in FileSeek %d", whence);
			break;
		}
	} else {
		
		return lseek(VfdCache[file].fd, offset, whence);
	}
	elog(WARN,"should not be here in FileSeek #2");
	return 0L;
}

long
fileTell (file)
	FileNumber	file;
{
	DO_DB(printf("DEBUG: FileTell %d (%s)\n",file,VfdCache[file].fileName));
	return FileSeek(file, 0, L_INCR);
}

int
fileSync (file)
	FileNumber	file;
{
	int	returnCode;
	FileAccess(file);
	returnCode = fsync(VfdCache[file].fd);
	return returnCode;
}

/*
 * keep track of how many have been allocated....   give a
 * warning if there are too few left
 */

private allocatedFiles = 0;

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
	close (fd);
	if (MAXFILES - ++allocatedFiles < 6) 
		elog(DEBUG,"warning: few useable file descriptors left (%d)", 
			MAXFILES - allocatedFiles);
	
	DO_DB(printf("DEBUG: AllocatedFile.  FreeFd = %d\n",FreeFd));
}

/* 
 * XXXX 7/5/88  When a transaction aborts after opening a file
 * the file will NOT be closed.  Since we don't have source to
 * lisp, we can't get lisp to go through FileNameOpenFile and thus
 * this will happen.
 * 
 * So, if you run out of file descriptors after having a bunch of
 * transactions abort, this is why.  This needs to be fixed.
 */

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

File
FileNameOpenFile(fileName, fileFlags, fileMode)
FileName fileName;
int fileFlags;
int fileMode;
{
    int i;
    File sfd;
    Sfd *sfdP;
    char *fname, *filepath();
    if (SfdCache == NULL) {
       SfdCache = (Sfd*)malloc(MAXNOFILES * sizeof(Sfd));
       for (i=0; i<MAXNOFILES-1; i++)
	 SfdCache[i].nextFree = i + 1;
       SfdCache[MAXNOFILES - 1].nextFree = 0;
    }
    sfd = SfdCache[0].nextFree;
    if (sfd == 0)  {
       elog(WARN, "out of striped file descriptors");
    } else {
    SfdCache[0].nextFree = SfdCache[sfd].nextFree;
    sfdP = &(SfdCache[sfd]);
    for (i=0; i<NStriping; i++) {
	fname = filepath(fileName, i);
	sfdP->vfd[i] = fileNameOpenFile(fname, fileFlags, fileMode);
    }
    sfdP->curStripe = 0;
    return(sfd);
    }
}

void
FileClose(file)
File file;
{
    int i;
    Sfd *sfdP;

    sfdP = &(SfdCache[file]);
    for (i=0; i<NStriping; i++)
	fileClose(sfdP->vfd[i]);
    sfdP->nextFree = SfdCache[0].nextFree;
    SfdCache[0].nextFree = file;
}

Amount
FileRead(file, buffer, amount)
File file;
String buffer;
Amount amount;
{
   Sfd *sfdP;
   Amount ret;
#ifdef PARALLELDEBUG
   usclk_t st;
   st = getusclk();
   FileReadCount++;
#endif
   sfdP = &(SfdCache[file]);
   ret = fileRead(sfdP->vfd[sfdP->curStripe], buffer, amount);
   sfdP->curStripe = (sfdP->curStripe + 1) % NStriping;
#ifdef PARALLELDEBUG
   FileReadTime += (getusclk() - st);
#endif
   return(ret);
}

Amount
FileWrite(file, buffer, amount)
File file;
String buffer;
Amount amount;
{
    Sfd *sfdP;
    Amount ret;
    sfdP = &(SfdCache[file]);
    ret = fileWrite(sfdP->vfd[sfdP->curStripe], buffer, amount);
    sfdP->curStripe = (sfdP->curStripe + 1) % NStriping;
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
    BlockNumber bnum;
    int nf;
    Sfd *sfdP;

    sfdP = &(SfdCache[file]);

    switch(whence) {
    case L_SET:
	sfdP->seekPos = offset;
        blknum = offset / BLCKSZ;
        blkoff = offset % BLCKSZ;
        bnum = blknum / NStriping;
        sfdP->curStripe = nf = blknum % NStriping;
	fileSeek(sfdP->vfd[nf], bnum * BLCKSZ + blkoff, whence);
	return offset;
    case L_INCR:
	sfdP->seekPos += offset;
        blknum = sfdP->seekPos / BLCKSZ;
        blkoff = sfdP->seekPos % BLCKSZ;
        bnum = blknum / NStriping;
        sfdP->curStripe = nf = blknum % NStriping;
	fileSeek(sfdP->vfd[nf], bnum * BLCKSZ + blkoff, whence);
	return sfdP->seekPos;
    case L_XTND:
	{   int l=0, h=NStriping-1, m;
            long lsize, hsize, msize;
	    if (h == 0) {
	        sfdP->curStripe = 0;
                sfdP->seekPos = fileSeek(sfdP->vfd[0], offset, whence);
	        return sfdP->seekPos;
	      }
            if ((lsize = fileSeek(sfdP->vfd[l], 0l, L_XTND)) < 0) {
                elog(FATAL, "lseek:%m");
            }
            if ((hsize = fileSeek(sfdP->vfd[h], 0l, L_XTND)) < 0) {
                elog(FATAL, "lseek:%m");
            }
            if (lsize == hsize)
               nf = 0;
            else {
                while (l + 1 != h) {
                    m = (l + h) / 2;
                if ((msize = fileSeek(sfdP->vfd[m], 0l, L_XTND)) < 0) {
                    elog(FATAL, "lseek:%m");
                }
                    if (msize > hsize)
                       l = m;
                    else
                       h = m;
                  }
                nf = h;
              }
	    sfdP->curStripe = nf;
            sfdP->seekPos = hsize * NStriping + nf * BLCKSZ + offset;
	    fileSeek(sfdP->vfd[nf], offset, whence);
	    return sfdP->seekPos;
	}
    }
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

char *
filepath(filename, stripe)
char *filename;
int stripe;
{
    char *buf;
    int len;

    len = strlen(PostgresHomes[stripe]) + strlen("/data/base/") 
	  + strlen(DBName) + strlen(filename) + 2;
    buf = (char*) palloc(len);
    sprintf(buf, "%s/data/base/%s/%s", PostgresHomes[stripe], DBName, filename);
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
    Sfd *sfdP;

    sfdP = &(SfdCache[file]);

    for (i=0; i<NStriping; i++)
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

    /* return(ret); */
    return(s);

}

