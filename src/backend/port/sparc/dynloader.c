
/*

(Message inbox:4)
Return-Path: jsmith@king.mcs.drexel.edu
Received: from king.mcs.drexel.edu by postgres.Berkeley.EDU (5.61/1.29)
	id AA08681; Wed, 29 Aug 90 13:54:19 -0700
Received: by king.mcs.drexel.edu (4.0/SMI-4.0)
	id AA13668; Wed, 29 Aug 90 16:54:44 EDT
Date: Wed, 29 Aug 90 16:54:44 EDT
From: jsmith@king.mcs.drexel.edu (Justin Smith)
Message-Id: <9008292054.AA13668@king.mcs.drexel.edu>
To: post_questions@postgres.berkeley.edu
Subject: Dynamic loader for Sparc systems
Status: O



I recently sent a message to postgres_bugs suggesting a way to do
dynamic loading on a Sparc system -- one that should be fairly
system-independant.  Here is a sample program that uses this:
there is a main program 'loader.c' that calls 'dynamic_load' and
loads a subprogram called 'tst.o'.  The program 'loader.c' must be
compiled with the 'static' option, i.e. the command to compile it
must be 

cc -Bstatic loader.c

when this is executed by typing a.out it loads tst.o and calls it.


Here is 'loader.c':

*/

#include <pwd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/file.h>

#ifndef MAXPATHLEN
# define MAXPATHLEN 1024
#endif

extern char pg_pathname[];

#include <a.out.h>

#include "utils/dynamic_loader.h"

/* 
 *
 *   ld -N -x -A SYMBOL_TABLE -o TEMPFILE FUNC -lc
 *
 *   ld -N -x -A SYMBOL_TABLE -T ADDR -o TEMPFILE FUNC -lc
 *
 */

static char *temp_file_name = NULL;
static char *path = "/usr/tmp/postgresXXXXXX";

DynamicFunctionList *
dynamic_file_load(err, filename, address, size)

char **err, *filename, **address;
long *size;

{
	extern end;
	extern char *mktemp();
	extern char *valloc();

	int nread;
	struct exec ld_header, header;
	unsigned long image_size, zz;
	char *load_address = NULL;
	FILE *temp_file = NULL;
	char command[256];
	DynamicFunctionList *retval = NULL, *load_symbols();
	int fd;


/* commented out -lc */

	fd = open(filename, O_RDONLY);

	read(fd, &ld_header, sizeof(struct exec));

	image_size = ld_header.a_text + ld_header.a_data + ld_header.a_bss;

	if (!(load_address = valloc(zz=image_size)))
	{
		*err = "unable to allocate memory";
		goto finish_up;
	}

	if (temp_file_name == NULL)
	{
		temp_file_name = (char *)malloc(strlen(path) + 1);
	}

	strcpy(temp_file_name,path);
	mktemp(temp_file_name);

	sprintf(command,"ld -N  -A %s -T %lx -o %s  %s -lc -lm -ll",
	    pg_pathname,
	    load_address,
	    temp_file_name,  filename);

	if(system(command))
	{
		*err = "link failed!";
		goto finish_up;
	}

	if(!(temp_file = fopen(temp_file_name,"r")))
	{
		*err = "unable to open tmp file";
		goto finish_up;
	}
	nread = fread(&header, sizeof(header), 1, temp_file);
	image_size = header.a_text + header.a_data + header.a_bss;
	if (zz<image_size)
	{
		*err = "loader out of phase!";
		goto finish_up;
	}

	fseek(temp_file, N_TXTOFF(header), 0);
	nread = fread(load_address, zz=(header.a_text + header.a_data),1,temp_file);
	/* zero the BSS segment */
	while (zz<image_size)
		load_address[zz++] = 0;

	retval = load_symbols(fd, &ld_header, load_address);

	fclose(temp_file);
	unlink(temp_file_name);
	*address = load_address;
	*size = image_size;

	temp_file = NULL;
	load_address = NULL;

finish_up:
	if (temp_file != NULL) fclose(temp_file);
	if (load_address != NULL) free(load_address);
	return retval;
}

DynamicFunctionList *
load_symbols(fd, hdr, entry_addr)

int fd;
struct exec *hdr;
int entry_addr;

{
	char *strings, *symb_table, *p, *q;
	int symtab_offset, string_offset, string_size, nsyms, i;
	struct nlist *table_entry;
	int entering = 1;
	DynamicFunctionList *head, *scanner;

	symtab_offset = N_SYMOFF(*hdr);
	string_offset = N_STROFF(*hdr);

	lseek(fd, string_offset, 0);
	read(fd, &string_size, sizeof(string_size));
	strings = (char *) malloc(string_size - 4);
	read(fd, strings, string_size - 4);
	nsyms = hdr->a_syms / sizeof(struct nlist);
	lseek(fd, symtab_offset, 0);
	symb_table = (char *) malloc(hdr->a_syms);
	read(fd, symb_table, hdr->a_syms);

	p = symb_table;
	for (i = 0; i < nsyms; i++)
	{
		table_entry = (struct nlist *) p;
		p += sizeof(struct nlist);
	    if (! ((table_entry->n_type & N_EXT) == 0
			|| (table_entry->n_type & N_TYPE) != N_TEXT))
		{
			if (entering)
			{
				head = (DynamicFunctionList *)
					   malloc(sizeof(DynamicFunctionList));
				scanner = head;
				entering = 0;
			}
			else
			{
				scanner->next = (DynamicFunctionList *)
								malloc(sizeof(DynamicFunctionList));
				scanner = scanner->next;
			}
			/*
			 * Add one for "_", ie
			 * overpaid() will be _overpaid
			 */

			q = strings + (table_entry->n_un.n_strx - 4) + 1;

			strcpy(scanner->funcname, q);
			scanner->func = (func_ptr) (table_entry->n_value + entry_addr);
			scanner->next = NULL;
		}
	}

	free(symb_table);
	free(strings);
	return(head);
}

func_ptr
dynamic_load(err)

char **err;

{
	*err = "Dynamic load: Should not be here!";
	return(NULL);
}
