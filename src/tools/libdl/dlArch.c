/*
 * dlArch.c-- 
 *     handles loading of library archives.
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
#include <strings.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "dl.h"
#include "dlPriv.h"

/*
 * Sometimes, you might want to have undefined symbols searched from 
 * standard libraries like libc.a and libm.a automatically. dl_setLibraries
 * is the interface to do this. dl_stdLibraries contain an arrary of
 * strings specifying the libraries to be searched.
 */
char **dl_stdLibraries= NULL;

static int searchArchive();
static int readArchiveRanlibs();
static int archGetObjectName();
static int archLoadObject();
static int loadArchSections();

/*****************************************************************************
 *                                                                           *
 *     Searching of pre-set libraries                                        *
 *                                                                           *
 *****************************************************************************/

/*
 * dl_searchLibraries returns 1 if undefined symbols are found during the
 * searching. 0 otherwise.
 */
int dl_searchLibraries()
{
    char **libs= dl_stdLibraries;
    int result= 0;

    if (dl_stdLibraries && _dl_undefinedSymbolCount) {
	while(*libs) {
	    result|= searchArchive(*libs);
	    libs++;
	}
    }
    return (result);
}

/*
 * dl_setLibraries--
 *      takes a string of the form <library>[:<library> ... ] which
 *      specifies the libraries to be searched automatically when there are
 *      undefined symbols.
 *
 *      eg. dl_setLibraries("/usr/lib/libc_G0.a:/usr/lib/libm_G0.a");
 */
void dl_setLibraries( libs )
     char *libs;
{
    char *name, *t;
    char **stnlib;
    int numlibs= 0;	
    int maxlibs= 4;

    if(!libs)
	return;
    stnlib= (char **)malloc(sizeof(char *) * maxlibs);
    name=t= libs;
    while(*t!='\0') {
	while(*t!=':' && *t!='\0')
	    t++;
	if (t-name>0) {
	    stnlib[numlibs]= strncpy((char*)malloc(t-name+1),name,t-name);
	    stnlib[numlibs++][t-name]='\0';
	    if(numlibs==maxlibs-1) {
		maxlibs*= 2;
		stnlib= (char **)realloc(stnlib, sizeof(char *) * maxlibs);
	    }
	}
	if (*t==':') {
	    t++;
	    name= t;
	}
    }
    stnlib[numlibs]= NULL;
    if (dl_stdLibraries) {
	char **s= dl_stdLibraries;
	while(*s!=NULL) {
	    free(*s);
	    s++;
	}
	free(dl_stdLibraries);
    }
    dl_stdLibraries= stnlib;

    return;
}

/*****************************************************************************
 *                                                                           *
 *       Internal Rountines                                                  *
 *                                                                           *
 *****************************************************************************/

static int searchArchive( archname )
     char *archname;
{
    int found= 0, done;
    struct ranlib *pran;
    char *pstr;
    int i;
    int fd;

    /*
     * opens the archive file and reads in the ranlib hashtable
     */
    if ((fd=readArchiveRanlibs(archname, &pran, &pstr)) < 0) {
	fprintf(stderr, "dl: cannot open \"%s\"", archname);
	return 0;
    }
    /*
     * look through our symbol hash table and see if we find anything.
     * We have to scan until no undefined symbols can be found in the
     * archive. (Note that bringing in an object file might require another
     * object file in the archive. We'll have missed the symbol if we
     * do this one pass and the symbol happens to be inserted into buckets
     * we've examined already.)
     */
    do {
	done= 1;
	for(i=0; i < HASHTABSZ; i++) {
	    HEnt *ent= dlHashTable[i];
	    struct ranlib *r;
	    while(ent) {
		if (!ent->symbol->objFile) {
		    r= (struct ranlib *)ranlookup(ent->symbol->name);
		    if (r->ran_off) {
			/*
			 * we've found the undefined symbol in the archive
			 */
#if DEBUG
			printf("*** found %s in ", ent->symbol->name);
#endif
			if (archLoadObject(fd, r->ran_off)) {
			    found= 1;
			    done= 0;
			}
		    }
		}
		ent=ent->next;
	    }
	}
    } while (!done);
    /*
     * be a good citizen.
     */
    free(pran);
    free(pstr);
    close(fd);

    return found;
}

/*
 * readArchiveRanlibs--
 *     opens a library and reads in the ranlib hash table and its
 *     associated string table. It returns -1 if fails, the opened
 *     file descriptor otherwise. It also inits the ranhashtable.
 */
static int readArchiveRanlibs( archfile, pran, pstr )
     char *archfile; struct ranlib **pran; char **pstr;
{
    int numRanlibs, numStrings;
    struct ranlib *ranlibs;
    char *strings;
    ARHDR  ar_hdr;
    int fd, size;
    char mag[SARMAG];

    *pran= NULL;
    *pstr= NULL;
    /* 
     * opens the library and check the magic string
     */
    if ((fd= open(archfile, O_RDONLY)) < 0 ||
	read(fd, mag, SARMAG)!=SARMAG ||
	strncmp(mag, ARMAG, SARMAG)!=0) {
	close(fd);
	return -1;
    }
    /*
     * reads in the archive header (not used) and the number of ranlibs.
     */
    if (read(fd, &ar_hdr, sizeof(ARHDR))!=sizeof(ARHDR) ||
	read(fd, &numRanlibs, sizeof(int))!= sizeof(int)) {
	close(fd);
	return -1;
    }
    /*
     * reads in the ranlib hash table and the string table size.
     */
    size= sizeof(struct ranlib)*numRanlibs;
    ranlibs= (struct ranlib *)malloc(size);
    if (read(fd, ranlibs, size)!=size ||
	read(fd, &numStrings, sizeof(int))!=sizeof(int)) {
	close(fd);
	return -1;
    }
    /*
     * reads in the string table.
     */
    strings= (char *)malloc(numStrings);
    if (read(fd, strings, numStrings)!=numStrings) {
	close(fd);
	return -1;
    }
    *pran= ranlibs;
    *pstr= strings;
    ranhashinit(ranlibs, strings, numRanlibs);
    return fd;
}

static int archLoadObject( fd, offset )
     int fd; int offset;
{
    dlFile *dlfile;
    FILHDR filhdr;
    ARHDR arhdr;
    char ar_name[17];

    if (lseek(fd, offset, SEEK_SET)==-1 ||
	read(fd, &arhdr, sizeof(ARHDR))!=sizeof(ARHDR) ||
	read(fd, &filhdr, sizeof(filhdr))!=sizeof(filhdr))
	return 0;
    sscanf(arhdr.ar_name, "%16s", ar_name);
    ar_name[16]='\0';
#if DEBUG
    printf("%.16s\n", ar_name);
#endif

    dlfile= (dlFile *)malloc(sizeof(dlFile));
    bzero(dlfile, sizeof(dlFile));
    dlfile->filename= STRCOPY(ar_name);
    dlfile->relocStatus= DL_NEEDRELOC;

    if (!_dl_loadSymbols(dlfile, fd, filhdr, offset+sizeof(ARHDR)) ||
	!_dl_loadSections(dlfile, fd, offset+sizeof(ARHDR))) {
	_dl_closeObject(dlfile);
	return 0;
    }

    if (!_dl_enterExternRef(dlfile)) {
	_dl_closeObject(dlfile);
	return 0;
    }

    /*
     * need to relocate now and see if we need to bring in more files.
     */
    _dl_relocateSections(dlfile);

    return 1;
}

dlFile *_dl_loadEntireArchive( filename, fd )
     char *filename; int fd; 
{
    dlFile *dlfile, *df;
    int offset;
    FILHDR filhdr;
    ARHDR arhdr;
    int size;
    struct stat stat_buf;

    /*
     * read in the header of the symbol list (the so-called symdef)
     */
    if (lseek(fd, SARMAG, SEEK_SET)==-1 ||
	read(fd, &arhdr, sizeof(arhdr))!=sizeof(arhdr)) {
	return 0;
    }
    /*
     * go after each member of the archive:
     */
    fstat(fd, &stat_buf);
    sscanf(arhdr.ar_size, "%d", &size);
    offset= SARMAG + sizeof(ARHDR) + size;
    dlfile= NULL;
    while( offset < stat_buf.st_size) {
	if (!lseek(fd, offset, SEEK_SET)==-1 ||
	    read(fd, &arhdr, sizeof(arhdr))!=sizeof(arhdr) ||
	    read(fd, &filhdr, sizeof(filhdr))!=sizeof(filhdr)) {
	    _dl_closeObject(dlfile);
	    return NULL;
	}
	offset+= sizeof(ARHDR);
	if (!(df=_dl_openObject(fd, filename, filhdr, offset, DL_LAZY))) {
	    _dl_closeObject(dlfile);
	    return NULL;
	}
	sscanf(arhdr.ar_size, "%d", &size);
	offset+= size;
	df->next= dlfile;
	dlfile= df;
    }

    return dlfile;
}
