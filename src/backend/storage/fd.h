
; /*
; * 
; * POSTGRES Data Base Management System
; * 
; * Copyright (c) 1988 Regents of the University of California
; * 
; * Permission to use, copy, modify, and distribute this software and its
; * documentation for educational, research, and non-profit purposes and
; * without fee is hereby granted, provided that the above copyright
; * notice appear in all copies and that both that copyright notice and
; * this permission notice appear in supporting documentation, and that
; * the name of the University of California not be used in advertising
; * or publicity pertaining to distribution of the software without
; * specific, written prior permission.  Permission to incorporate this
; * software into commercial products can be obtained from the Campus
; * Software Office, 295 Evans Hall, University of California, Berkeley,
; * Ca., 94720 provided only that the the requestor give the University
; * of California a free licence to any derived software for educational
; * and research purposes.  The University of California makes no
; * representations about the suitability of this software for any
; * purpose.  It is provided "as is" without express or implied warranty.
; * 
; */



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
 *
 * Identification:
 *	$Header$
 */

#ifndef	FDIncluded	/* Include this file only once */
#define FDIncluded	1

#include "c.h"

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

#endif	/* !defined(FDIncluded) */
