/*
 * fd.h --
 *	Virtual file descriptor definitions.
 *
 * calls:
 * 
 *  File {Close, Read, Write, Seek, Tell, Sync}
 *  {File Name Open, Allocate, Free} File
 *
 * These are NOT JUST RENAMINGS OF THE UNIX ROUTINES.
 * use them for all file activity...
 *
 *  fd = FilePathOpenFile("foo", O_RDONLY);
 *  File fd;
 *
 * use AllocateFile if you need a file descriptor in some other context.
 * it will make sure that there is a file descriptor free
 *
 * use FreeFile to let the virtual file descriptor package know that 
 * there is now a free fd (when you are done with it)
 *
 *  AllocateFile();
 *  FreeFile();
 */

#ifndef	FDIncluded		/* Include this file only once */
#define FDIncluded	1

/*
 * Identification:
 */
#define FD_H	"$Header$"

#ifndef C_H
#include "c.h"
#endif

typedef String	FileName;

#ifdef _xprs_
#define NSTRIPING	5	/* no. of disks hermes has */
typedef struct { int fd[NSTRIPING]; bool is_sys; } File;
#else /* _xprs_ */
typedef int	File;
#endif /* _xprs_ */
typedef int	FileNumber;
typedef int	Amount;

/*
 * FileNameOpenFile --
 *
 */
/* VARARGS2 */
extern
FileNumber
FileNameOpenFile ARGS((
	FileName	fileName,
	int		flags,
	int		mode
));

/*
 * FileClose --
 *
 */
extern
void
FileClose ARGS((
	FileNumber	file
));

/*
 * FileUnlink --
 *
 */
extern
void
FileUnlink ARGS((
	FileNumber	file
));

/*
 * FileRead --
 *
 */
extern
Amount
FileRead ARGS((
	FileNumber	file,
	String	buffer,
	Size	amount
));

/*
 * FileWrite --
 *
 */
extern
Amount
FileWrite ARGS((
	FileNumber	file,
	String	buffer,
	Size	amount
));

/* UNIX DEPENDENT XXXX 6/21/88 */

/*
 * FileSeek --
 *
 */
extern
long		/* XXX FilePosition? */
FileSeek ARGS((
	FileNumber	file,
	long	offset,
	int	whence
));


/*
 * FileTell --
 *
 */
extern
long		/* XXX FilePosition? */
FileTell ARGS((
	FileNumber	file
));

/*
 * FileSync --
 *
 *  fsync		(UNIX)
 */

extern
int
FileSync ARGS((
	FileNumber	file
));

/*
 * AllocateFile --
 *
 */
extern
void
AllocateFile ARGS((
	void
));

/*
 * AllocateFiles --
 *
 */
extern
uint16
AllocateFiles ARGS((
	uint16	fileCount
));

/*
 * FreeFile --
 *
 */
extern
void
FreeFile ARGS((
	void
));

/*
 * FreeFiles --
 *
 */
extern
void
FreeFiles ARGS((
	uint16	numberOfFiles
));

#endif	/* !defined(FDIncluded) */
