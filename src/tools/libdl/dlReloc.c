/*
 *  dlReloc.c--
 *      handles the relocation 
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
#include <sys/mman.h>
#include "dl.h"
#include "dlPriv.h"

static void patchLongjump();
static void protectText();

int _dl_relocateSections(dlfile)
     dlFile *dlfile;
{
    long textAddress= dlfile->textAddress - dlfile->textVaddr;
    long rdataAddress= dlfile->rdataAddress - dlfile->rdataVaddr;
    long dataAddress= dlfile->dataAddress - dlfile->dataVaddr;
    long bssAddress= dlfile->bssAddress - dlfile->bssVaddr;
    int i, j;
    int hasUndefined= 0;
    pEXTR extsyms= dlfile->extsyms;
    char *extSs= dlfile->extss;

    if (dlfile->relocStatus==DL_RELOCATED)	/* just in case */
	return 1;

    /* prevent circular relocation */
    dlfile->relocStatus= DL_INPROG;

    for(i=0; i < dlfile->nsect; i++) {
	SCNHDR *hdr= &(dlfile->sect[i].hdr);
	RELOC *relocEnt= dlfile->sect[i].relocEntries;
	long sectAddr= dlfile->sect[i].addr - hdr->s_vaddr;
	long relocAddr;
	int *addr;
	int undefinedCount= 0;

	for(j= 0; j < hdr->s_nreloc; j++) {
	    if (relocEnt->r_extern) {
		pEXTR esym= &extsyms[relocEnt->r_symndx];
		char *symname= extSs + esym->asym.iss;
		dlSymbol *symbol;

		symbol= dl_hashSearchSymbol(symname);
		if(!symbol || !symbol->objFile) {
		    RELOC *rents= dlfile->sect[i].relocEntries;
		    int found;

		    if (j!=undefinedCount)
			rents[undefinedCount]=rents[j];
		    if (!symbol) {
			(void)dl_hashInsertSymbolStrIdx(STRCOPY(symname), 0,
							&found);
			_dl_undefinedSymbolCount++;
		    }
		    undefinedCount++;
		    relocEnt++;
		    continue;	/* skip this one */
		}
		if(symbol->objFile->relocStatus==DL_NEEDRELOC) {
		    /*
		     * trigger an avalanche of relocates! (In fact, we
		     * don't need to relocate unless the symbol references
		     * unrelocated text but we do it anyway.) 
		     *
		     * if we fail to relocate the object file containing the
		     * symbol, we treat it as undefined and keep it around
		     * to trigger relocation next time.
		     */
		    if (!_dl_relocateSections(symbol->objFile)) {
			RELOC *rents= dlfile->sect[i].relocEntries;

			if (j!=undefinedCount)
			    rents[undefinedCount]=rents[j];
			undefinedCount++;
			relocEnt++;
			continue;	/* skip this one */
		    }
		}
		relocAddr= symbol->addr;
	    }else {
		switch(relocEnt->r_symndx) {
		case R_SN_TEXT:
		    relocAddr= textAddress;
		    break;
		case R_SN_RDATA:
		    relocAddr= rdataAddress;
		    break;
		case R_SN_DATA:
		    relocAddr= dataAddress;
		    break;
		case R_SN_BSS:
		    relocAddr= bssAddress;
		    break;
		case R_SN_NULL:
		case R_SN_SDATA:
		case R_SN_SBSS:
		    _dl_setErrmsg("unknown section %d referenced",
			    relocEnt->r_symndx);
		    return 0;
		    break;
		case R_SN_INIT:
		case R_SN_LIT8:
		case R_SN_LIT4:
		    /*
		     * I've never encounter these. (-G 0 should kill the
		     * LIT4's and LIT8's. I'm not sure if INIT is even used.)
		     */
		    _dl_setErrmsg("section %d not implemented",
			    relocEnt->r_symndx);
		    return 0;
		    break;
		default:
		    fprintf(stderr, "dl: unknown section %d\n",
			    relocEnt->r_symndx);
		}
	    }
	    addr= (int *)(sectAddr + relocEnt->r_vaddr);
	    switch(relocEnt->r_type) {
	    case R_ABS:
		break;
	    case R_REFWORD:
		*addr += relocAddr;
		break;
	    case R_JMPADDR:
		patchLongjump(dlfile, addr, relocAddr, relocEnt->r_vaddr);
		break;
	    case R_REFHI: {
		RELOC *nxtRent= relocEnt+1;
		if (nxtRent->r_type != R_REFLO) {
		    /* documentation says this will not happen: */
		    fprintf(stderr, "dl: R_REFHI not followed by R_REFLO\n");

		    /* 
		     * use old way-- just relocate R_REFHI. This will break if
		     * R_REFLO has a negative offset. 
		     */
		    if((short)(relocAddr&0xffff) < 0) {
			*addr += (((unsigned)relocAddr>>16)+ 1);
		    }else {
			*addr += ((unsigned)relocAddr>> 16);
		    }
		}else {
		    int hi_done= 0;
		    int hi_newaddr=0;
		    /*
		     * documentation lies again. You can have more than
		     * one R_REFLO following a R_REFHI.
		     */
		    while(j<hdr->s_nreloc && nxtRent->r_type==R_REFLO) {
			int *lo_addr= (int *)(sectAddr + nxtRent->r_vaddr);
			int oldaddr, newaddr;
			int temphi;	

			oldaddr= ((*addr)<<16) + (short)((*lo_addr) & 0xffff);
			newaddr= relocAddr + oldaddr;
			if((short)(newaddr&0xffff) < 0) {
			    temphi= (((unsigned)newaddr>>16)+ 1);
			}else {
			    temphi= ((unsigned)newaddr>> 16);
			}
			if(!hi_done) {
			    hi_newaddr= temphi;
			    hi_done=1;
			}else {
			    if(temphi!=hi_newaddr) {
				fprintf(stderr, "dl: REFHI problem: %d %d don't match\n",
				    temphi, hi_newaddr);
			    }
			}
			*lo_addr &= 0xffff0000;
			*lo_addr |= (newaddr & 0xffff);
			j++; /* the following R_REFLO(s) has been relocated */
			relocEnt++;
			nxtRent++;	      
		    }
		    *addr &= 0xffff0000;	/* mask the immediate fields */
		    *addr |= (hi_newaddr & 0xffff);
		}
		break;
	    }
	    case R_REFLO:
		/* 
		 * shouldn't be here (REFHI should have taken care of these)
		 * -- just in case 
		 */
		fprintf(stderr, "dl: warning: dangling R_REFLO.\n");
		*addr += (relocAddr & 0xffff);
		break;
	    case R_GPREL:
		fprintf(stderr,"dl: Hopeless: $gp used.\n");	    
		break;
	    default:
		fprintf(stderr,"dl: This local relocation not implemented yet.\n");
	    }
	    relocEnt++;
	}
	hdr->s_nreloc= undefinedCount;
	if(undefinedCount>0) {
	    hasUndefined= 1;
	}else {
	    free(dlfile->sect[i].relocEntries);
	    dlfile->sect[i].relocEntries= NULL;
	}
    }
    dlfile->relocStatus= hasUndefined? DL_NEEDRELOC : DL_RELOCATED;
    if(!hasUndefined) {
	free(dlfile->extsyms);
	dlfile->extsyms= NULL;
	dlfile->iextMax= 0;
	protectText(dlfile);
    }
    return (!hasUndefined);
}

/*
 * patchLongjump patches R_JMPADDR references. The problem is that the
 * immediate field is only 28 bits and the references are often out of
 * range. We need to jump to a near place first (ie. the jmptable here)
 * and do a "jr" to jump farther away.
 */
static void patchLongjump( dlfile, addr, relocAddr, vaddr )
    dlFile *dlfile; int *addr; long relocAddr; long vaddr;
{
    int *patch, instr;
    JmpTbl *jmptable= dlfile->jmptable;
    JmpTblHdr *jhdr= (jmptable) ? (JmpTblHdr *)jmptable->block : NULL;

    if (!jmptable || jhdr->current==jhdr->max) {
	int pagesize;

	/* need new jump table */
	jmptable= (JmpTbl *)malloc(sizeof(JmpTbl));
	pagesize= getpagesize();
	jmptable->block= (char *)valloc(pagesize);
	bzero(jmptable->block, pagesize);
	jmptable->next= dlfile->jmptable;
	jhdr= (JmpTblHdr *)jmptable->block;
	jhdr->current= 0;
	jhdr->max= (pagesize - sizeof(JmpTblHdr))/16;
	dlfile->jmptable= jmptable;
    }

    if ((unsigned)addr>>28!=relocAddr>>28) {
	patch= (int *)jmptable->block + jhdr->current*4 + 4;
	jhdr->current++;
	if ((unsigned)patch>>28!=(unsigned)patch>>28) {
	    fprintf(stderr,"dl: out of luck! Can't jump.\n");
	    return;
	}
	if ((*addr)&0x3ffffff) {
	    relocAddr+= (*addr & 0x3ffffff)<<2;
	}
	if (vaddr) {
	    relocAddr+= (vaddr & 0xf0000000);
	}
	if (relocAddr&0x3) {
	    fprintf(stderr,"dl: relocation address not word-aligned!\n");
	}
	/* lui $at, hiOffset */
	*patch= 0x3c010000 | (relocAddr>>16);	   
	/* ori $at, $at, loOffset */
	*(patch+1)=0x34210000|(relocAddr&0xffff);  
	/* jr $at */
	*(patch+2)= 0x00200008;		
	/* nop */
	*(patch+3)= 0;			
	*addr &= 0xfc000000;	/* leave the jal */
	*addr |= (((long)patch>>2) & 0x3ffffff);
    }else {
	if (relocAddr&0x3) {
	    fprintf(stderr,"dl: relocation address not word-aligned!\n");
	}
	*addr += (relocAddr>>2) & 0x3ffffff;
    }
    return;
}

/*
 * change memory protection so that text and the jumptables cannot be
 * accidentally overwritten.
 */
static void protectText( dlfile )
     dlFile *dlfile;
{
    int pagesize= getpagesize();
    JmpTbl *jmptable= dlfile->jmptable;

    if (dlfile->textAddress) {
	/* protect the text */
	if (mprotect((char *)dlfile->textAddress, dlfile->textSize,
		      PROT_EXEC) == -1) {
	    fprintf(stderr, "dl: fail to protect text of %s\n", dlfile->filename);
	}
	/* protect jump tables, if any */
	while(jmptable) {
	    if (mprotect((char *)jmptable->block, pagesize, 
			 PROT_EXEC) == -1) {
		fprintf(stderr, "dl: fail to protect a jump table of %s\n", 
			dlfile->filename);
	    }
	    jmptable= jmptable->next;
	}
    }
    return;
}

