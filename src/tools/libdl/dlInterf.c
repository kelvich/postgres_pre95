/*
 *  dlInterf.c--
 *      implements the dl* interface
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
#include <varargs.h>
#include "dl.h"
#include "dlPriv.h"

static char errmsg[500];		/* the error message buffer */

static int _dl_openFile();

#define	OBJECT_MAGIC(x) \
    ((x)==MIPSEBMAGIC || (x)==MIPSELMAGIC || (x)==SMIPSEBMAGIC || \
     (x)==SMIPSELMAGIC || (x)==MIPSEBUMAGIC || (x)==MIPSELUMAGIC)

/*****************************************************************************
 *                                                                           *
 *    dl_init, dl_open, dl_sym, dl_close, dl_error interface routines        *
 *                                                                           *
 *****************************************************************************/

/*
 * dl_init--
 *      takes the pathname of the current executable and reads in the 
 *      symbols. It returns 1 if successful and 0 otherwise.
 */
int dl_init( filename )
     char *filename;
{
    int fd;
    dlFile *dlfile;
    FILHDR filhdr;

    if (!filename) return 0;

    /*
     * open the executable for extracting the symbols
     */
    if((fd=_dl_openFile(filename, &filhdr, 0)) < 0) {
	_dl_setErrmsg("cannot open \"%s\"", filename);
	return 0;
    }
    /*
     * create a dlFile entry for the executable
     */
    dlfile= (dlFile *)malloc(sizeof(dlFile));
    bzero(dlfile, sizeof(dlFile));
    dlfile->filename= STRCOPY(filename);
    dlfile->relocStatus= DL_RELOCATED;

    /*
     * load in and enter the symbols
     */
    _dl_hashInit();
    if (!_dl_loadSymbols(dlfile, fd, filhdr, 0)) {
	_dl_closeObject(dlfile);
	_dl_setErrmsg("Cannot load symbol table from \"%s\"", filename);
	return 0;
    }
    if (!_dl_enterInitialExternRef(dlfile)) {
	_dl_closeObject(dlfile);
	return 0;
    }

    close(fd);

    return 1;
}

/*
 * dl_open--
 *     opens the object file or library archive specified by filename
 *     It can be opened with either DL_LAZY or DL_NOW mode. DL_LAZY does
 *     no perform symbol resolution until a symbol in the file is accessed.
 *     DL_NOW performs symbol resolution at load time. It returns a
 *     handle if successful, NULL otherwise.
 */
void *dl_open( filename, mode )
     char *filename; int mode;
{
    int fd;
    dlFile *dlfile;
    FILHDR filhdr;

    if((fd=_dl_openFile(filename, &filhdr, 0)) < 0) {
	_dl_setErrmsg("cannot open \"%s\"", filename);
	return NULL;
    }

    /*
     * determine whether we have an object file or a library
     */
    if (OBJECT_MAGIC(filhdr.f_magic)) {
	/*
	 * an ordinary object file.
	 */
	dlfile= _dl_openObject(fd, filename, filhdr, 0, mode);
	close(fd);
    }else if (!strncmp((char*)&filhdr, ARMAG, SARMAG)) {
	dlFile *df;

	/*
	 * a library: load in every object file.
	 */
	if ((dlfile=_dl_loadEntireArchive(filename, fd))==NULL) {
	    close(fd);
	    _dl_setErrmsg("Cannot load archive \"%s\"", filename);
	    return NULL;
	}

	/*
	 * do the relocation now if mode==DL_NOW. (note that we couldn't
	 * relocate in the above loop since archive members might reference
	 * each other.)
	 */
	if (mode==DL_NOW) {
	    int search= 0;

	    df= dlfile;
	    while(df) {
		if (!_dl_relocateSections(df))
		    search= 1;
		df= df->next;
	    }
	    if (search) {
		if (!dl_searchLibraries()) {
		    _dl_setErrmsg("\"%s\" contains undefined symbols", 
			      filename);
		    _dl_closeObject(dlfile);
		    return NULL;
		}
		df= dlfile;	/* one more time */
		while(df) {
		    if (!_dl_relocateSections(df)) {
			_dl_setErrmsg("\"%s\" contains undefined symbols", 
				      filename);
			_dl_closeObject(dlfile);
			return NULL;
		    }
		    df= df->next;
		}
	    }
	}
    }else {
	_dl_setErrmsg("\"%s\" neither an object file nor an archive",
		      filename);
    }

    return (void *)dlfile;
}


/*
 * dl_sym--
 *      returns the location of the specified symbol. handle is not
 *      actually used. It returns NULL if unsuccessful. 
 */
void *dl_sym( handle, name )
     void *handle; char *name;
{
    dlFile *dlfile;
    dlSymbol *symbol;

    symbol = dl_hashSearchSymbol(name);
    if (symbol) {
	dlfile= symbol->objFile;
	/* 
	 * might have undefined symbols or have not been relocated yet.
	 */
	if (dlfile->relocStatus==DL_NEEDRELOC) {
	    if (!_dl_relocateSections(dlfile)) {
		if (dl_searchLibraries()) {
		    /* find some undefined symbols, try again! */
		    _dl_relocateSections(dlfile);
		}
	    }
	}
	/* 
	 * only returns the symbol if the relocation has completed
	 */
	if (dlfile->relocStatus==DL_RELOCATED)
	    return (void *)symbol->addr;
    }
    if (symbol) {
	_dl_setErrmsg("\"%s\" has undefined symbols", dlfile->filename);
    }else {
	_dl_setErrmsg("no such symbol \"%s\"", name);
    }
    return NULL;
}

/*
 * dl_close--
 *      closes the file and deallocate all resources hold by the file.
 *	note that any references to the deallocated resources will result
 *      in undefined behavior.
 */
void dl_close( handle )
     void *handle;
{
    _dl_closeObject((dlFile *)handle);
    return;
}

/*
 * dl_error--
 *      returns the error message string of the previous error.
 */
char *dl_error()
{
    return errmsg;
}

/*****************************************************************************
 *                                                                           *
 *    Object files handling Rountines                                        *
 *                                                                           *
 *****************************************************************************/

dlFile *_dl_openObject( fd, filename, filhdr, offset, mode )
     int fd; char *filename; FILHDR filhdr; int offset; int mode;
{ 
    dlFile *dlfile;

    dlfile= (dlFile *)malloc(sizeof(dlFile));
    bzero(dlfile, sizeof(dlFile));
    dlfile->relocStatus= DL_NEEDRELOC;
    dlfile->filename= STRCOPY(filename);

    if (!_dl_loadSymbols(dlfile, fd, filhdr, offset) ||
	!_dl_loadSections(dlfile, fd, offset)) {
	_dl_setErrmsg("Cannot load symbol table or sections from \"%s\"",
		      filename);
	_dl_closeObject(dlfile);
	return NULL;
    }

    if (!_dl_enterExternRef(dlfile)) {
	_dl_closeObject(dlfile);
	return NULL;
    }

    if(mode==DL_NOW) {
	if (!_dl_relocateSections(dlfile)) {
	    /*
	     * attempt to search the "standard" libraries before aborting
	     */
	    if (!dl_searchLibraries() || 
		!_dl_relocateSections(dlfile)) {

		_dl_setErrmsg("\"%s\" contains undefined symbols", 
			      filename);
		_dl_closeObject(dlfile);
		return NULL;
	    }
	}
    }else {
	dlfile->relocStatus= DL_NEEDRELOC;
    }

    return dlfile;
}

void _dl_closeObject( dlfile )
     dlFile *dlfile;
{
    int i;
    dlFile *next;

    while(dlfile) {
	next= dlfile->next;
	if (dlfile->filename) 
	    free(dlfile->filename);
	if (dlfile->sect) 
	    free(dlfile->sect);
	if (dlfile->extss) 
	    free(dlfile->extss);
	if (dlfile->extsyms)
	    free(dlfile->extsyms);
	/* frees any symbols associated with it */
	for(i=0; i < HASHTABSZ; i++) {
	    HEnt *ent= dlHashTable[i], *prev, *t;
	    prev= dlHashTable[i];
	    while(ent) {
		if (ent->symbol->objFile==dlfile) {
		    t= ent->next;
		    if (prev==dlHashTable[i]) {
			dlHashTable[i]= prev= ent->next;
		    }else {
			prev->next= ent->next;
		    }
		    free(ent);
		    ent= t;
		}else {
		    prev= ent;
		    ent=ent->next;
		}
	    }
	}
	free(dlfile);
	dlfile= next;
    }
}

int _dl_loadSymbols( dlfile, fd, filhdr, offset )
     dlFile *dlfile; int fd; FILHDR filhdr; int offset;
{
    SCNHDR *scnhdr;
    HDRR symhdr;
    char *pssext;
    pEXTR pext;
    int nscn, size, i;

    /*
     * load in section headers (don't need this for the executable during
     * init)
     */
    if (dlfile->relocStatus!=DL_RELOCATED) {
	nscn= filhdr.f_nscns;
	scnhdr= (SCNHDR *)malloc(sizeof(SCNHDR) * nscn);
	if (lseek(fd, filhdr.f_opthdr, SEEK_CUR)==-1 ||
	    read(fd, scnhdr, sizeof(SCNHDR)*nscn)!= sizeof(SCNHDR)*nscn)
	    return 0;
    }
    /*
     * load in symbolic header
     */
    if (lseek(fd, offset+filhdr.f_symptr, SEEK_SET)==-1 ||
	read(fd, &symhdr, sizeof(symhdr))!=sizeof(symhdr) ||
	symhdr.magic!=magicSym)
	return 0;
    /*
     * read external strings table
     */
    size= symhdr.issExtMax;
    pssext= (char *)malloc(size);
    if (lseek(fd, offset+symhdr.cbSsExtOffset, SEEK_SET)==-1 ||
	read(fd, pssext, size)!=size) 
	return 0;
    /*
     * read external symbols table
     */
    size= symhdr.iextMax * sizeof(EXTR);
    pext= (pEXTR)malloc(size);
    if (lseek(fd, offset+symhdr.cbExtOffset, SEEK_SET)==-1 ||
	read(fd, pext, size)!=size) 
	return 0;
    /*
     * copy the extern string space and symbols. The string space is 
     * referenced by the hash table. We need the symbols either in
     * lazy resolution or when there are undefined symbols.
     */
    dlfile->extss= pssext;
    dlfile->issExtMax= symhdr.issExtMax;
    dlfile->extsyms= pext;
    dlfile->iextMax= symhdr.iextMax;
    /*
     * relocStatus should only be DL_RELOCATED for the executable's dlfile
     * during init. (Don't need to relocate the executable!)
     */
    if (dlfile->relocStatus==DL_RELOCATED)
	return 1;
    /*
     * otherwise, create the ScnInfo's
     */
    dlfile->nsect= nscn;
    size= sizeof(ScnInfo)*nscn;
    dlfile->sect= (ScnInfo *)malloc(size);
    bzero(dlfile->sect, size);
    for(i=0; i < nscn; i++) {
	dlfile->sect[i].hdr = scnhdr[i];
    }
    free(scnhdr);
    return 1;
}

/*
 * _dl_loadSections--
 *      loads all sections of an object file into memory.
 */
int _dl_loadSections(dlfile, fd, offset)
     dlFile *dlfile; int fd; int offset;
{
    int nsect= dlfile->nsect;
    int i;
    RELOC *relocEnt;
    int size;

    for(i=0; i < nsect; i++) {
	SCNHDR *hdr= &(dlfile->sect[i].hdr);
	char *sectnam;
	char *addr;
	int isBss= 0;

	sectnam= hdr->s_name;
	if(!strncmp(sectnam, ".text", 5)) {
	    int pagesize= getpagesize();
	    int size= (hdr->s_size%pagesize==0)? hdr->s_size :
		hdr->s_size - hdr->s_size % pagesize + pagesize;

	    /* page aligned */
	    addr= (char *)valloc(size);
	    dlfile->textAddress= (CoreAddr)addr;
	    dlfile->textVaddr= hdr->s_vaddr;
	    dlfile->textSize= size;
	}else {
	    addr= (char *)malloc(hdr->s_size);
	    if (!strncmp(sectnam, ".rdata", 6)) {
		dlfile->rdataAddress= (CoreAddr)addr;
		dlfile->rdataVaddr= hdr->s_vaddr;
	    }else if (!strncmp(sectnam, ".data", 5)) {
		dlfile->dataAddress= (CoreAddr)addr;
		dlfile->dataVaddr= hdr->s_vaddr;
	    }else if (!strncmp(sectnam, ".bss", 4)) {
		dlfile->bssAddress= (CoreAddr)addr;
		dlfile->bssVaddr= hdr->s_vaddr;
		bzero(addr, hdr->s_size);	/* zero out bss segment */
		isBss= 1;
	    }
	}
	dlfile->sect[i].addr= (CoreAddr)addr;
	if(!isBss) {
	    /* 
	     * read in raw data from the file 
	     */
	    if (hdr->s_size &&
		(lseek(fd, offset+hdr->s_scnptr, SEEK_SET)==-1 ||
		 read(fd, addr, hdr->s_size)!=hdr->s_size))
		return 0;
	    /*
	     * read in relocation entries from the file
	     */
	    if (hdr->s_nreloc) {
		size= sizeof(RELOC)* hdr->s_nreloc;
		dlfile->sect[i].relocEntries= relocEnt=
		    (RELOC *)malloc(size);
		if (lseek(fd, offset+hdr->s_relptr, SEEK_SET)==-1 ||
		    read(fd, relocEnt, size)!=size)
		    return 0;
	    }
	}
    }
    return 1;
}

static int _dl_openFile( filename, fhdr, offset )
     char *filename; FILHDR *fhdr; int offset;
{
    int fd;
    if ((fd=open(filename, O_RDONLY)) >= 0) {
	/*
	 * load in file header
	 */
	if (lseek(fd, offset, SEEK_SET)==-1 ||
	    read(fd, fhdr, sizeof(FILHDR))!=sizeof(FILHDR))
	    return -1;

    }
    return fd;
}


_dl_setErrmsg( va_alist )
     va_dcl
{
    va_list pvar;
    char *fmt;

    va_start(pvar);
    fmt= va_arg(pvar, char *);
    vsprintf(errmsg, fmt, pvar);
    va_end(pvar);
}
