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
 */

#define RESERVEFORLISP	5

/*
 * CURRENT HACK
 *
 * Problem: you cannot rebind open() when lisp call it.  We therefore need
 * to guarentee that there are file descriptors free for lisp to use.
 * 
 * The current solution is to limit the number of files descriptors 
 * that this code will allocated at one time.  (it leaves RESERVEDFORLISP
 * free)
 */

#include <sys/file.h>
#include <sys/param.h>
#include <errno.h>

extern errno;

#define	MAXFILES	(NOFILE-RESERVEFORLISP)

#include "c.h"

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

#include "fd.h"

#define FileClosed -1

#include "fd.h"
#include "log.h"

#define FileIsNotOpen(file) (VfdCache[file].fd == FileClosed)

typedef struct vfd {
	signed short	fd;
	File	nextFree;
	File	lruMoreRecently;
	File	lruLessRecently;
	long	seekPos;
	char	*fileName;
	int	fileFlags;
	int	fileMode;
} Vfd;

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

private	FreeFd = 0;

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
	File	file
));

private
int
LruInsert ARGS((
	File	file
));

private
void
AssertLruRoom();

private File
AllocateVfd();

private inline void
Delete (file)
	File	file;
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
	File	file;
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
	File	file;
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
	File	file;
{
	Vfd	*vfdP;
	int	returnValue;

	DO_DB(printf("DEBUG:	LruInsert %d (%s)\n",file,VfdCache[file].fileName));

	vfdP = &VfdCache[file];

	if (FileIsNotOpen(file)) {

		/*
		 * Open the requested file
		 */

tryAgain:
		vfdP->fd = open(vfdP->fileName,vfdP->fileFlags,vfdP->fileMode);

		if (vfdP->fd < 0) {
			if (errno == EMFILE) {
				FreeFd = 0;
				errno = 0;
				AssertLruRoom();
				goto tryAgain;
			} else {
				return vfdP->fd;
			}
		} else {
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

private inline void
AssertLruRoom()
{
	DO_DB(printf("DEBUG:	AssertLruRoom (FreeFd = %d)\n",FreeFd));

	if (FreeFd <= 0 || nfile >= MAXFILES) {
		LruDelete(VfdCache[0].lruMoreRecently);
	}
}

private int
FileAccess(file)
	File	file;
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

private	File
AllocateVfd()
{
	Index	i;
	File	file;
	
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
	File	file;
{
	DO_DB(printf("DEBUG:	FreeVfd: %d (%s)\n",file,VfdCache[file].fileName));

	VfdCache[file].nextFree = VfdCache[0].nextFree;
	VfdCache[0].nextFree = file;
}

/* VARARGS2 */
File
FileNameOpenFile(fileName, fileFlags, fileMode) 
	FileName	fileName;
	int		fileFlags;
	int		fileMode;
{
	static int osRanOut = 0;
	File	file;
	Vfd	*vfdP;
	
	DO_DB(printf("DEBUG: FileNameOpenFile: %s %x %o\n",fileName,fileFlags,fileMode));

	file = AllocateVfd();
	vfdP = &VfdCache[file];

	if (nfile >= MAXFILES || (FreeFd == 0 && osRanOut)) {
		AssertLruRoom();
	}

tryAgain:

	vfdP->fd = open(fileName,fileFlags,fileMode);

	if (vfdP->fd < 0) {

		if (errno == EMFILE) {
			errno = 0;
			FreeFd = 0;
			osRanOut = 1;
			AssertLruRoom();
			goto tryAgain;
		} else {
			FreeVfd(file);
			return -1;
		}
	} else {
		++nfile;
	}
	
	(void)LruInsert(file);

	vfdP->fileName = malloc(strlen(fileName)+1);
	strcpy(vfdP->fileName,fileName);

	vfdP->fileFlags = fileFlags & ~(O_TRUNC|O_EXCL);
	vfdP->fileMode = fileMode;

	return file;
}

void
FileClose(file)
	File	file;
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

Amount
FileRead (file, buffer, amount)
	File	file;
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
FileWrite (file, buffer, amount)
	File	file;
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
FileSeek (file, offset, whence)
	File	file;
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
FileTell (file)
	File	file;
{
	DO_DB(printf("DEBUG: FileTell %d (%s)\n",file,VfdCache[file].fileName));
	return FileSeek(file, 0, L_INCR);
}

int
FileSync (file)
	File	file;
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
