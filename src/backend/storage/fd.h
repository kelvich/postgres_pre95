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

typedef int	File;
typedef int	Amount;

/*
 * FileNameOpenFile --
 *
 */
/* VARARGS2 */
extern
File
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
	File	file
));

/*
 * FileRead --
 *
 */
extern
Amount
FileRead ARGS((
	File	file,
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
	File	file,
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
	File	file,
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
	File	file
));

/*
 * FileSync --
 *
 *  fsync		(UNIX)
 */

extern
int
FileSync ARGS((
	File	file
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

#define FD_SYMBOLS \
	SymbolDecl(FileNameOpenFile, "_FileNameOpenFile"), \
	SymbolDecl(FileClose, "_FileClose"), \
	SymbolDecl(FileRead, "_FileRead"), \
	SymbolDecl(FileWrite, "_FileWrite"), \
	SymbolDecl(FileSeek, "_FileSeek"), \
	SymbolDecl(FileTell, "_FileTell"), \
	SymbolDecl(FileSync, "_FileSync"), \
	SymbolDecl(AllocateFile, "_AllocateFile"), \
	SymbolDecl(AllocateFiles, "_AllocateFiles"), \
	SymbolDecl(FreeFile, "_FreeFile"), \
	SymbolDecl(FreeFiles, "_FreeFiles")

#endif	/* !defined(FDIncluded) */
