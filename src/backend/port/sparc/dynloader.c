/*
 *   DYNAMIC_LOADER.C
 *
 *   Dynamically load specified object module
 *
 *   TODO!
 *	-remember loaded object's symbols so objects are not loaded more
 *	 than once (which means they use a different set of static and
 *	 glboal variables
 *
 *	-BSS space variables do not work (i.e., static or global
 *	 declarations which are not assigned
 *
 *	 int a;		// doesn't work
 *	 int b = 1; 	// works
 */

#include <stdio.h>
#include <fcntl.h>
#ifndef CIncluded
#include "c.h"
#endif
#include "log.h"
#include "dynamic_loader.h"

#include <a.out.h>
#include <alloca.h>

/*
#ifdef sun
#define N_DATAOFF(hdr)	(N_TXTOFF(hdr) + (hdr).a_text)
#define N_TROFF(hdr)	(N_DATAOFF(hdr) + (hdr).a_data)
#define N_DROFF(hdr)	(N_TROFF(hdr) + (hdr).a_trsize)
#endif
*/

RcsId("$Header$");

#ifdef NOTDEF
typedef struct reloc_info_sparc	Reloc;
typedef struct nlist		NList;

char *sbrk();
char *Align();

#define MASK(n) ((1<<(n))-1)
#define	IN_RANGE(v,n)   ((-(1<<((n)-1))) <=(v) && (v) < (1<<((n)-1)))

static struct exec hdr;
#endif

func_ptr
dynamic_load(err, filename, funcname)
char **err;
char *filename;
char *funcname;
{
	err = "Dynamic loading not supported on Sparcs";
	return(NULL);
}

#ifdef NOTDEF
    int fd;
    int n;
    int a_strsize;
    char *p_start;
    char *p_text;
    char *p_data;
    char *p_bss;
    char *p_syms;
    char *p_treloc;
    char *p_dreloc;
    char *p_strs;
    char *p_end;
    func_ptr entryadr = NULL;	/* entry address of function */

    elog(DEBUG, "dynamic_load %s %s\n", filename, funcname);

    AllocateFile();	/* ensure a file descriptor is free for use */
    fd = open(filename, O_RDONLY, 0);
    if (fd < 0) {
	*err = "unable to open file";
	return(NULL);
    }
    n = read(fd, &hdr, sizeof(hdr));
    if (n != sizeof(hdr) /*|| N_BADMAG(hdr)*/) {
	*err = "bad object header";
	close(fd);
	return(NULL);
    }
    /*
     *  BRK enough for:
     *		a_text
     *		a_data
     *		a_bss
     *		a_syms
     *		a_trsize
     *		a_drsize
     *
     *  When through relocating, BRK to remove a_syms, a_trsize, a_drsize
     *  NOTE THAT NO CALLS THAT USE MALLOC() MAY BE MADE AFTER THE SBRK!
     */

    a_strsize = lseek(fd, 0, 2) - N_STROFF(hdr);

    p_start = sbrk(0);
    p_text = Align(p_start);
    p_data = p_text + hdr.a_text;
    p_bss  = p_data + hdr.a_data;
    p_syms = Align(p_bss  + hdr.a_bss);
    p_treloc = Align(p_syms + hdr.a_syms);
    p_dreloc = Align(p_treloc + hdr.a_trsize);
    p_strs   = Align(p_dreloc + hdr.a_drsize);
    p_end    = Align(p_strs + a_strsize);

    if (p_end < p_strs) {
	*err = "format error";
	close(fd);
	return(NULL);
    }
    if (brk(p_end) == -1) {
	*err = "brk() failed";
	close(fd);
	return(NULL);
    }
    n = seekread("text", fd, N_TXTOFF(hdr), p_text, hdr.a_text);
    n += seekread("data", fd, N_DATOFF(hdr), p_data, hdr.a_data);
    n += seekread("syms", fd, N_SYMOFF(hdr), p_syms, hdr.a_syms);
    n += seekread("trel", fd, N_TRELOFF(hdr), p_treloc, hdr.a_trsize);
    n += seekread("drel", fd, N_DRELOFF(hdr), p_dreloc, hdr.a_drsize);
    n += seekread("strs", fd, N_STROFF(hdr), p_strs, a_strsize);
    close(fd);
    if (n) {
	*err = "format-read error";
        if (sbrk(0) != p_end) 
	    elog(WARN, "dynamic_load: unexpected malloc");
	else
	    brk(p_start);	/* restore allocated memory */
	return(NULL);
    }

    /*
     *  Find entry symbol
     */

    {
	NList *s;
	NList *syme = (NList *)(p_syms + hdr.a_syms);

	for (s = (NList *)p_syms; s < syme; ++s) {
	    char *str = p_strs + s->n_un.n_strx;
	    if ((s->n_type & N_EXT) == 0 || (s->n_type & N_TYPE) != N_TEXT)
		continue;
	    if (strcmp(str, funcname) == 0) {
		entryadr = (func_ptr)(p_text + s->n_value);
		break;
	    }
	    if (*str == '_' && strcmp(str + 1, funcname) == 0) {
		entryadr = (func_ptr)(p_text + s->n_value);
		break;
	    }
	}
    }

    /*
     *  relocate
     *
     *  n holds cumulative error
     */

    {
	NList *syms = (NList *)p_syms;
	Reloc *reloc;
	Reloc *relend;

	n = 0;
	reloc = (Reloc *)p_treloc;
	relend= (Reloc *)(p_treloc + hdr.a_trsize);
	while (reloc < relend) {
	    n += relocate(p_text,reloc, p_syms, p_strs, p_text, p_data, p_bss);
	    ++reloc;
	}
	
	reloc = (Reloc *)p_dreloc;
	relend= (Reloc *)(p_dreloc + hdr.a_drsize);
	while (reloc < relend) {
	    n += relocate(p_data,reloc, p_syms, p_strs, p_text, p_data, p_bss);
	    ++reloc;
	}
    }
    if (n)
	*err = "relocate error";
    if (entryadr == NULL)
	*err = "entry pt. not found";

    if (sbrk(0) != p_end) {
	elog(WARN, "unexpected malloc %08lx %08lx", sbrk(0), p_end);
    } else {
        brk(p_syms);	/* destroy non essential data */
    }
    return(entryadr);
}

relocate(base, reloc, sym, strs, tbase, dbase, bbase)
char *base;
Reloc *reloc;
NList *sym;
char  *strs;
char  *tbase, *dbase, *bbase;
{
    NList *symbase = sym;
    unsigned long value;
    char *addr;
    short n_type;

#ifndef	sparc
    sym += reloc->r_symbolnum;		/* only valid if r_extern */
#else
    sym += reloc->r_index;
#endif

#ifndef	sparc
    switch(reloc->r_length) {
    case 0:
	value = *(unsigned char *)(base + reloc->r_address);
	break;
    case 1:
	value = *(unsigned short *)(base + reloc->r_address);
	break;
    case 2:
	value = *(unsigned long *)(base + reloc->r_address);
	break;
    default:
	return(-1);
    }
#else
    value = reloc->r_addend;
#endif

#ifdef sequent
    if (reloc->r_bsr)
	value = -value;
#endif

    /*
    printf("reloc %08lx sym %d %d%d%d%d%d vadr %08lx (symbol %02x %08lx %s)\n",
	reloc->r_address,
	reloc->r_symbolnum,
	reloc->r_pcrel,
	1 << reloc->r_length,
	reloc->r_extern,
#ifdef sequent
	reloc->r_bsr,
	reloc->r_disp,
#else
	0,0,
#endif
	value,
	sym->n_type,
	sym->n_value,
	(reloc->r_extern) ? strs + sym->n_un.n_strx : "null"
    );
    */

#ifndef	sparc
    if (reloc->r_extern)
	n_type = sym->n_type & N_TYPE;
    else
	n_type = reloc->r_symbolnum & N_TYPE;
#else
    n_type = sym->n_type & N_TYPE;
#endif

    switch(n_type) {
    case N_UNDF:
	{
	    FList *fl = ExtSyms;
	    while (fl->name) {
		if (strcmp(fl->name, strs + sym->n_un.n_strx) == 0)
		    break;
		++fl;
	    }
	    if (fl->name == NULL) {
		elog(WARN, "dynamic_loader: Illegal ext. symbol %s",
		    strs + sym->n_un.n_strx
		);
		return(-1);
	    }
	    value += (long)fl->func;
	    /*
	    printf("func %08lx value %08lx\n", fl->func, value);
	    */
	}
	break;
    case N_ABS:
	value += sym->n_value;
	break;
    case N_TEXT:
	value += (long)tbase;
	break;
    case N_DATA:
	value += (long)tbase;
	break;
    case N_BSS:
	value += (long)bbase;
	break;
    default:
	return(-1);
	/*
	printf("Unknown type %02x\n", sym->n_type);
	*/
	break;
    }

#ifdef sequent
    if (reloc->r_bsr)
	value = value - (long)tbase;
#endif

#ifndef	sparc
    if (reloc->r_pcrel)
	value = value - (long)base;
#else
    if ( reloc->r_type == RELOC_PC10 || reloc->r_type == RELOC_PC22 )
	value = value - (long)base;
#endif

#ifndef	sparc
    switch(reloc->r_length) {
    case 0:
	*(unsigned char *)(base + reloc->r_address) = value;
	break;
    case 1:
	*(unsigned short *)(base + reloc->r_address) = value;
	break;
    case 2:
	*(unsigned long *)(base + reloc->r_address) = value;
	break;
    }
#else
    addr = base + reloc->r_address;
    switch ( reloc->r_type ){

    case RELOC_8:
    case RELOC_DISP8:
	if ( IN_RANGE ( value, 8 ))
		*(unsigned char *) addr = value;
	break;
    case RELOC_LO10:
    case RELOC_PC10:
    case RELOC_BASE10:
	*(unsigned long *)addr = (*(long *)addr & ~MASK(10))|(value & MASK(10));
	break;
    case RELOC_BASE13:
    case RELOC_13:
	*(unsigned long *)addr = (*(long *)addr & ~MASK(13))|(value & MASK(13));
	break;
    case RELOC_DISP16:
    case RELOC_16:
	if ( IN_RANGE ( value, 16 ))
		*(unsigned short *)addr = value;
	break;
    case RELOC_22:
	if ( IN_RANGE ( value, 22 ))
		*(unsigned long *)addr = (*(long *)addr & ~MASK(22)) |
						(value & MASK(22));
	break;
    case RELOC_HI22:
    case RELOC_BASE22:
    case RELOC_PC22:
	*(unsigned long *)addr = (*(long *)addr & ~MASK(22)) |
						((value>>(32-22)) & MASK(22));
	break;
    case RELOC_WDISP22:
	value >> = 2;
	if ( IN_RANGE ( value, 22 ))
		*(unsigned long *)addr = (*(long *)addr & ~MASK(22)) |
						(value & MASK(22));
	break;
    case RELOC_JMP_TBL:
    case RELOC_WDISP30:
	value >> = 2;
	*(unsigned long *)addr = (*(long *)addr & ~MASK(30)) |
						(value & MASK(30));
	break;
    case RELOC_32:
    case RELOC_DISP32:
	*(unsigned long *)addr = value;
	break;
    }
#endif

    return(0);
}

seekread(seg, fd, offset, ptr, bytes)
char *seg;
int fd;
int offset, bytes;
unsigned char *ptr;
{
    int n;

    n = lseek(fd, offset, 0);
    if (n != offset)
	return(-1);
    n = read(fd, ptr, bytes);
    if (n != bytes)
	return(-1);
    return(0);
}

char *
Align(padr)
char *padr;
{
    long adr = ((long)padr + 3) & ~3;
    return((char *)adr);
}
#endif
