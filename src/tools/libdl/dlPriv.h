/*
 *  dlPriv.h--
 *      the Private header file for the Dynamic Loader Library. Normal
 *      users should have no need to include this file.
 *
 *
 *  Copyright (c) 1993 Andrew K. Yu, University of California at Berkeley
 *  All rights reserved.
 *
 *  Permission to use, copy, modify, and distribute this software and its
 *  documentation for educational, research, and non-profit purposes and
 *  without fee is hereby granted, provided that the above copyright
 *  notice appear in all copies and that both that copyright notice and
 *  this permission notice appear in supporting documentation. Permission
 *  to incorporate this software into commercial products can be obtained
 *  from the author. The University of California and the author make
 *  no representations about the suitability of this software for any 
 *  purpose. It is provided "as is" without express or implied warranty. 
 *
 */
#ifndef _DL_PRIV_HEADER_
#define _DL_PRIV_HEADER_

extern dlSymbol *dl_hashSearchSymbol();
extern dlSymbol *dl_hashInsertSymbolStrIdx();

#define STRCOPY(x)  (char *)strcpy((char *)malloc(strlen(x)+1), x)

#define HASHTABSZ	1001

typedef struct HEnt_ {
    dlSymbol *symbol;
    struct HEnt_ *next;
} HEnt;

typedef struct JmpTblHdr {
    int	current;		/* current empty slot */
    int max;			/* max no. of slots */
    int dummy[2];		/* padding to make this 4 words */
} JmpTblHdr;

extern HEnt **dlHashTable;
extern int _dl_undefinedSymbolCount;

extern dlFile *_dl_openObject();
extern void _dl_closeObject();

extern int _dl_loadSections();
extern int _dl_loadSymbols();

dlFile *_dl_loadEntireArchive();

#endif  _DL_PRIV_HEADER_
