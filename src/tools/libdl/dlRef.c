/*
 *  dlRef.c--
 *      handles symbol references
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
#include <stdio.h>
#include "dl.h"
#include "dlPriv.h"

HEnt **dlHashTable;			/* hash table for the symbols */
int _dl_undefinedSymbolCount= 0;	/* number of undefined symbols */

static unsigned hash();

/*****************************************************************************
 *                                                                           *
 *     Hash Table for symbols                                                *
 *                                                                           *
 *****************************************************************************/

/*
 * hash is taken from Robert Sedgewick's "Algorithms in C" (p.233). Okay, so
 * this is not the most sophisticated hash function in the world but this
 * will do the job quite nicely.
 */
static unsigned hash( str )
     char *str;
{
    int h;
    for(h=0; *str!='\0'; str++)
	h = (64*h + *str) % HASHTABSZ;
    return h;
}

void _dl_hashInit()
{
    dlHashTable= (HEnt **)malloc(sizeof(HEnt *) * HASHTABSZ);
    bzero(dlHashTable, sizeof(HEnt *) * HASHTABSZ);
    return;
}

dlSymbol *dl_hashInsertSymbolStrIdx( extSs, idx, found )
     char *extSs; long idx; int *found;
{
    dlSymbol *symbol; 
    char *symname= extSs + idx;
    int hval= hash(symname);
    HEnt *ent, *prev, *e; 

    prev= e= dlHashTable[hval];
    while(e) {
	if(!strcmp(e->symbol->name, symname)) {
	    *found= 1;
	    return (e->symbol);   /* return existing symbol */
	}
	prev= e;
	e= e->next;
    }
    ent= (HEnt *)malloc(sizeof(HEnt));
    symbol= (dlSymbol *)malloc(sizeof(dlSymbol));
    bzero(symbol, sizeof(dlSymbol));
    symbol->name= symname;
    ent->symbol= symbol;
    ent->next= NULL;
    if (!prev) {
	dlHashTable[hval]= ent;
    }else {
	prev->next= ent;
    }
    *found= 0;
    return symbol;
}

dlSymbol *dl_hashSearchSymbol( symname )
     char *symname;
{
    int hval= hash(symname);
    HEnt *ent= dlHashTable[hval];
    while(ent) {
	if(!strcmp(ent->symbol->name, symname))
	    return ent->symbol;
	ent= ent->next;
    }
    return NULL;
}

/*****************************************************************************
 *                                                                           *
 *     Entering External References                                          *
 *                                                                           *
 *****************************************************************************/

int _dl_enterInitialExternRef( dlfile )
     dlFile *dlfile; 
{
    char *extSs= dlfile->extss;
    pEXTR extsyms= dlfile->extsyms;
    dlSymbol *symbol;
    int i, found;

    /*
     * this is done by init. Just enter the symbols and values:
     * (It cannot contain undefined symbols and the multiple defs.)
     */
    for(i=0; i < dlfile->iextMax ; i++) {
	pEXTR esym= &extsyms[i];
	
	symbol= dl_hashInsertSymbolStrIdx(extSs, esym->asym.iss, &found);
	if (found) {
	    _dl_setErrmsg("init error: symbol \"%s\" multiply defined",
			      extSs+esym->asym.iss);
	    return 0;
	}
	symbol->addr= esym->asym.value;
	symbol->objFile= dlfile;
    }
    free(dlfile->extsyms);
    dlfile->extsyms= NULL;
    dlfile->iextMax= 0;
    return 1;
}

int _dl_enterExternRef( dlfile )
     dlFile *dlfile;
{
    int i;
    char *extSs= dlfile->extss;
    dlSymbol *symbol;
    int found;
    long textAddress= (long)dlfile->textAddress - dlfile->textVaddr;
    long rdataAddress= (long)dlfile->rdataAddress - dlfile->rdataVaddr;
    long dataAddress= (long)dlfile->dataAddress - dlfile->dataVaddr;
    long bssAddress= (long)dlfile->bssAddress - dlfile->bssVaddr;
    
    for(i=0; i < dlfile->iextMax ; i++) {
	pEXTR esym= &dlfile->extsyms[i];
	int found;
	
	if (esym->asym.sc!=scNil && esym->asym.sc!=scUndefined) {
	    symbol= dl_hashInsertSymbolStrIdx(extSs, esym->asym.iss, 
					      &found);
	    if (symbol->objFile!=NULL) {
		_dl_setErrmsg("\"%s\" multiply defined", 
			      extSs+esym->asym.iss);
		return 0;
	    }
	    if (found) {
		/*
		 * finally, we now have the undefined symbol. (A kludge
		 * here: the symbol name of the undefined symbol is
		 * malloc'ed. We need to free it.)
		 */
		free(symbol->name);
		symbol->name= extSs+esym->asym.iss;
		_dl_undefinedSymbolCount--;
	    }
	    switch(esym->asym.sc) {
	    case scAbs:
		symbol->addr= esym->asym.value;
		break;
	    case scText:
		symbol->addr= textAddress + esym->asym.value;
		break;
	    case scData:
		symbol->addr= dataAddress + esym->asym.value;
		break;
	    case scBss:
		symbol->addr= bssAddress + esym->asym.value;
		break;
	    case scRData:
		symbol->addr= rdataAddress + esym->asym.value;
		break;
	    case scCommon: {
		char *block= (char *)malloc(esym->asym.value);
		bzero(block, esym->asym.value);
		symbol->addr= (long)block;
		break;
	    }
	    default:
		fprintf(stderr, "dl: extern symbol in unexpected section (%d)\n",
			esym->asym.sc);
		break;
	    }
	    symbol->objFile= dlfile;
	}
    }
    return 1;
}

/*****************************************************************************
 *                                                                           *
 *     Misc. utilities                                                       *
 *                                                                           *
 *****************************************************************************/

/*
 * dl_undefinedSymbols--
 *      returns the number of undefined symbols in count and an array of
 *      strings of the undefined symbols. The last element of the array
 *      is guaranteed to be NULL.
 */
char **dl_undefinedSymbols( count )
     int *count;
{
    char **syms= NULL;
    int i, j;

    *count= _dl_undefinedSymbolCount;
    if (_dl_undefinedSymbolCount) {
	syms= (char **)malloc(sizeof(char *) * (_dl_undefinedSymbolCount+1));
	for(i=0, j=0; i<HASHTABSZ && j<_dl_undefinedSymbolCount; i++) {
	    HEnt *ent= dlHashTable[i];
	    while(ent) {
		if (!ent->symbol->objFile) {
		    syms[j++]= STRCOPY(ent->symbol->name);
		    if (j==_dl_undefinedSymbolCount)
			break;
		}
		ent=ent->next;
	    }
	}
	syms[j]=NULL;
    }
    return syms;
}

/*
 * dl_printAllSymbols--
 *      aids debugging. Prints out symbols in the hash table that matches
 *      the file handle. For library archives, prints those that matches
 *      any member or the archive. A NULL handle matches everything.
 */
void dl_printAllSymbols( handle )
     void *handle;
{
    int i, count= 0;
    
    for(i=0; i < HASHTABSZ; i++) {
	HEnt *ent= dlHashTable[i];
	while(ent) {
	    if (!handle || handle==ent->symbol->objFile) {
		printf("(%3d) %-20s addr=0x%x dlfile=0x%x\n",
		       i, ent->symbol->name, ent->symbol->addr, 
		       ent->symbol->objFile);
		count++;
	    }else if (((dlFile *)handle)->next) {
		dlFile *dlfile= ((dlFile *)handle)->next;
		while(dlfile) {
		    if (dlfile==ent->symbol->objFile) {
			printf("(%3d) %-20s addr=0x%x dlfile=0x%x\n",
			       i, ent->symbol->name, ent->symbol->addr, 
			       ent->symbol->objFile);
			count++;
		    }
		    dlfile= dlfile->next;
		}
	    }
	    ent= ent->next;
	}
    }
    printf("total number of symbols= %d\n", count);
    return;
}
