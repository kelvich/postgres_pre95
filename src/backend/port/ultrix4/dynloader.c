
/*
 *   DYNAMIC_LOADER.C
 *
 *   Dynamically load specified object module
 *
 *   IDENTIFICATION:
 *   $Header$
 */

#include <stdio.h>
#include <fcntl.h>
#ifndef CIncluded
#include "c.h"
#endif
#include "log.h"
#include "dynamic_loader.h"

#include <sys/exec.h>
#include <reloc.h>
#include <sym.h>
#include <syms.h>
#include <symconst.h>

typedef struct reloc		Reloc;

char *sbrk();
char *Align();

/*
 *  WARNING: the exec header may show dsize = N and bsize = 0 where the
 *  actual format in 'dat' and 'bss' is N - V and V.
 */

static struct {
    struct exec ex;
    struct scnhdr txt;
    struct scnhdr dat;
    struct scnhdr bss;
} hdr;

/*
 *  Unfortunately, the MIPS proccessor's JAL (jump and link) instruction
 *  only has a 28 bit resolution (26 bits long world aligned).  Therefore,
 *  for dynamically loaded code in the data area (0x1000,0000) to access
 *  routines in the text area, a jump table must be constructed in the
 *  data area that uses the JR (jump register) instruction.
 */

typedef struct {
    long  ary[4];
    /*
     *  lui  r14,0xMMMM		0x3C0Exxxx
     *  ori  r14,r14,0xLLLL	0x35CExxxx
     *  jr   r14		0x01C00008  000000rrrrr00000... (r=01110)
     *  nop			0x00000000
     */
} AJMP;

#define AJMP0	0x3C0E0000
#define AJMP1	0x35CE0000
#define AJMP2	0x01C00008
#define AJMP3	0x00000000

static HDRR shdr;

#define a_nscns         ex.ex_f.f_nscns
#define a_symptr        ex.ex_f.f_symptr
#define a_nsyms         ex.ex_f.f_nsyms

#define a_tsize         ex.ex_o.tsize
#define a_dsize         ex.ex_o.dsize
#define a_bsize         ex.ex_o.bsize

AJMP	*AJmp;

func_ptr
dynamic_load(err, filename, funcname)
char **err;
char *filename;
char *funcname;
{
    int fd;
    int n;
    int a_strsize;
    char *ptr;
    char *p_start;
    char *p_text;
    char *p_data;
    char *p_bss;
    char *p_jmptab;
    char *p_syms;
    char *p_treloc;
    char *p_dreloc;
    char *p_breloc;
    char *p_strs;
    char *p_end;
    func_ptr entryadr = NULL;	/* entry address of function */

    elog(DEBUG, "dynamic_load %s %s\n", filename, funcname);

    fd = open(filename, O_RDONLY, 0);
    if (fd < 0) {
	*err = "unable to open file";
	return(NULL);
    }
    n = read(fd, &hdr.ex, sizeof(hdr.ex));
    if (n != sizeof(hdr.ex) /*|| N_BADMAG(hdr)*/) {
	*err = "bad object header";
	close(fd);
	return(NULL);
    }
    if (hdr.a_nscns > 3) {
	*err = "expected 3 sections in object hdr";
	close(fd);
	return(NULL);
    }
    read(fd, &hdr.txt, hdr.a_nscns * sizeof(hdr.txt));
    lseek(fd, hdr.a_symptr, 0);
    n = read(fd, &shdr, sizeof(shdr));
    if (n != sizeof(shdr)) {
	*err = "bad sym hdr";
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
     *		and string table
     *
     *  When through relocating, BRK to remove a_syms, a_trsize, a_drsize
     *  NOTE THAT NO CALLS THAT USE MALLOC() MAY BE MADE AFTER THE SBRK!
     */

    p_start = ptr = Align(sbrk(0));
    p_text  = ptr;	    ptr = p_text + hdr.txt.s_size;
    p_data  = ptr;	    ptr = p_data + hdr.dat.s_size;
    p_bss   = ptr;	    ptr = p_bss  + hdr.bss.s_size;
    p_jmptab= ptr;	    ptr = p_jmptab+ shdr.iextMax * sizeof(AJMP);
    p_syms  = Align(ptr);   ptr = p_syms + shdr.iextMax * sizeof(EXTR);
    p_treloc= Align(ptr);   ptr = p_treloc + hdr.txt.s_nreloc * sizeof(Reloc);
    p_dreloc= Align(ptr);   ptr = p_dreloc + hdr.dat.s_nreloc * sizeof(Reloc);
    p_breloc= Align(ptr);   ptr = p_breloc + hdr.bss.s_nreloc * sizeof(Reloc);
    p_strs  = Align(ptr);   ptr = p_strs + shdr.issExtMax;
    p_end   = Align(ptr);

    AJmp = (AJMP *)p_jmptab;

    if (brk(p_end) == -1) {
	*err = "brk() failed";
	close(fd);
	return(NULL);
    }
    n = 0;
    n += seekread("text", fd, hdr.txt.s_scnptr, p_text, hdr.txt.s_size);
    n += seekread("data", fd, hdr.dat.s_scnptr, p_data, hdr.dat.s_size);
    n += seekread("bss ", fd, hdr.bss.s_scnptr, p_bss , hdr.bss.s_size);
    n += seekread("syms", fd, shdr.cbExtOffset, p_syms, shdr.iextMax 
							* sizeof(EXTR));
    n += seekread("trel", fd, hdr.txt.s_relptr, p_treloc,
					hdr.txt.s_nreloc * sizeof(Reloc));
    n += seekread("drel", fd, hdr.dat.s_relptr, p_dreloc,
					hdr.dat.s_nreloc * sizeof(Reloc));
    n += seekread("brel", fd, hdr.bss.s_relptr, p_breloc,
					hdr.bss.s_nreloc * sizeof(Reloc));
    n += seekread("strs", fd, shdr.cbSsExtOffset, p_strs, shdr.issExtMax);
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
	EXTR *s = (EXTR *)p_syms;
	EXTR *e = s + shdr.iextMax;

	for (; s < e; ++s) {
	    char *str;
	    if (s->asym.sc != scText)
		continue;
	    str = p_strs + s->asym.iss;
	    if (strcmp(str, funcname) == 0) {
		entryadr = (func_ptr)(p_text + s->asym.value);
		break;
	    }
	    if (*str != '_')
		continue;
	    if (strcmp(str + 1, funcname) == 0) {
		entryadr = (func_ptr)(p_text + s->asym.value);
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
	EXTR *syms = (EXTR *)p_syms;
	Reloc *reloc;
	Reloc *relend;

	n = 0;
	reloc = (Reloc *)p_treloc;
	relend= reloc + hdr.txt.s_nreloc;
	while (reloc < relend) {
	    n += relocate(p_text, reloc, p_syms, p_strs, p_text, p_data, p_bss, hdr.txt.s_vaddr, hdr.dat.s_vaddr, hdr.bss.s_vaddr);
	    ++reloc;
	}

	reloc = (Reloc *)p_dreloc;
	relend= reloc + hdr.dat.s_nreloc;
	while (reloc < relend) {
	    n += relocate(p_data, reloc, p_syms, p_strs, p_text, p_data, p_bss, hdr.txt.s_vaddr, hdr.dat.s_vaddr, hdr.bss.s_vaddr);
	    ++reloc;
	}

	reloc = (Reloc *)p_breloc;
	relend= reloc + hdr.bss.s_nreloc;
	while (reloc < relend) {
	    n += relocate(p_bss, reloc, p_syms, p_strs, p_text, p_data, p_bss, hdr.txt.s_vaddr, hdr.dat.s_vaddr, hdr.bss.s_vaddr);
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

relocate(base, reloc, symbase, strs, tbase, dbase, bbase, vtbase, vdbase, vbbase)
char *base;
Reloc *reloc;
EXTR *symbase;
char  *strs;
char  *tbase, *dbase, *bbase;
long  vtbase, vdbase, vbbase;
{
    char *address = tbase + reloc->r_vaddr; /* always rel to tbase */
    char *symname;
    unsigned long value;
    short n_type;

    fflush(stdout);
#ifdef DYNAMIC_LOADER_DEBUG
    printf("relocate (tdb) %08lx %08lx %08lx addr %08lx\n",
	tbase, dbase, bbase, address
    );
#endif

    if (reloc->r_extern == 0) {
	symname = "<internal>";
	switch(reloc->r_symndx) {
	case R_SN_TEXT:
#ifdef DYNAMIC_LOADER_DEBUG
	    puts("local text");
#endif
	    value = (long)tbase - vtbase;
	    break;
	case R_SN_DATA:
#ifdef DYNAMIC_LOADER_DEBUG
	    puts("local data");
#endif
	    value = (long)dbase - vdbase;
	    break;
	case R_SN_SDATA:
#ifdef DYNAMIC_LOADER_DEBUG
	    puts("local sdata ???");
#endif
	    value = (long)bbase - vbbase;
	    break;
	default:
	    elog(WARN, "dynamic_loader: don't understand secton type %d",
		reloc->r_symndx
	    );
	    break;
	}
    } else {
        EXTR *sym = symbase + reloc->r_symndx;
	symname = strs + sym->asym.iss;
	switch(sym->asym.sc) {
	case scText:
#ifdef DYNAMIC_LOADER_DEBUG
	    puts("text symb");
#endif
	    value = (long)tbase - vtbase;
	    break;
	case scData:
#ifdef DYNAMIC_LOADER_DEBUG
	    puts("data symb");
#endif
	    value = (long)dbase - vdbase;
	    break;
	case scBss:
#ifdef DYNAMIC_LOADER_DEBUG
	    puts("bss symb");
	    puts("????");
#endif
	    value = (long)dbase - vbbase;
	    break;
	case scUndefined:
#ifdef DYNAMIC_LOADER_DEBUG
	    puts("undef symb");
#endif
	    {
		FList *fl = ExtSyms;
		char *str = strs + sym->asym.iss;
		AJMP *aj;
#ifdef DYNAMIC_LOADER_DEBUG
		puts(str);
#endif

		while (fl->name) {
	    	    if (strcmp(fl->name, str) == 0)
		        break;
		    if (fl->name[0] == '_' && strcmp(fl->name + 1, str) == 0)
			break;
		    ++fl;
		}
		if (fl->name == NULL)
		    elog(WARN, "dynamic_loader: Illegal ext. symbol %s", str);
		value = (long)fl->func;
#ifdef DYNAMIC_LOADER_DEBUG
		printf("func addr %08lx\n", value);
#endif

		aj =  AJmp + reloc->r_symndx;	/* index into jump table */
						/* create jt entry	 */
		aj->ary[0] = AJMP0 | ((value >> 16) & 0xFFFF);
		aj->ary[1] = AJMP1 | (value & 0xFFFF);
		aj->ary[2] = AJMP2;
		aj->ary[3] = AJMP3;
		value = (long)aj;
#ifdef DYNAMIC_LOADER_DEBUG
		printf("Jmp Tab Addr %08lx\n", value);
#endif
	    }
	    break;
	default:
	    elog(WARN, "dynamic_loader: do'na understand storage class %d",
		sym->asym.sc
	    );
	    break;
	}
    }

    switch(reloc->r_type) {
    case R_REFHALF:	/* 1 16 bit reference */
    case R_REFWORD:	/* 2 32 bit reference */
	elog(WARN, "dynamic_loader: reloc type? %d %s",
	    reloc->r_type, symname
	);
	break;
    case R_JMPADDR:	/* 3 26 bit jump ref (rel?)  */
	value &= 0x0FFFFFFF;
#ifdef DYNAMIC_LOADER_DEBUG
	printf("ja %08lx %08lx (%08lx)\n", address, value, *(long *)address);
#endif
	*(long *)address += (value >> 2);	/* long words */
#ifdef DYNAMIC_LOADER_DEBUG
	printf("ja address is: %08lx\n", (*(long *)address & 0x03FFFFFF) << 2);
#endif
	break;
    case R_REFHI:	/* 4 high 16 bits	    */
#ifdef DYNAMIC_LOADER_DEBUG
	printf("rhi %08lx %08lx (%s)\n", address, value, value);
#endif
	*(short *)(address) += (value >> 16) & 0xFFFF;
	break;
    case R_REFLO:	/* 5 low 16 bits	    */
#ifdef DYNAMIC_LOADER_DEBUG
	printf("rlo %08lx %08lx\n", address, value);
#endif
	*(short *)(address) += value & 0xFFFF;
	break;
    case R_GPREL:	/* 6 global pointer relative data item */
	elog(WARN, "dynamic_loader: must compile -G 0!",
	    reloc->r_type, symname
	);
	break;
    case R_LITERAL:	/* 7 global pointer relative literal pool item */
	elog(WARN, "dynamic_loader: reloc type? %d %s",
	    reloc->r_type, symname
	);
	break;
    default:
	return(-1);
    }
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

