/*

Return-Path: jsmith@king.mcs.drexel.edu

cc -Bstatic loader.c

when this is executed by typing a.out it loads tst.o and calls it.

!!!! Be sure that we close all open files up exit on the Sequent - the ld !!!!
     fails otherwise 

*/

#include <pwd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/file.h>

extern char pg_pathname[];

#include <a.out.h>

#include "utils/dynamic_loader.h"

/*
 * Allow extra space for "overruns" caused by the link.
 */

#define FUDGE 10000

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
	unsigned long image_size, true_image_size;
	char *load_address = NULL;
	FILE *temp_file = NULL;
	DynamicFunctionList *retval = NULL, *load_symbols();
	int fd;

/* commented out -lc */

	fd = open(filename, O_RDONLY);

	if (fd == -1)
	{
		*err = "error opening file";
		goto finish_up;
	}

	read(fd, &ld_header, sizeof(struct exec));

	image_size = ld_header.a_text + ld_header.a_data + ld_header.a_bss + FUDGE;

	close(fd); /* don't open it until the load is finished. */

	if (!(load_address = valloc(image_size)))
	{
		*err = "unable to allocate memory";
		goto finish_up;
	}

	if (temp_file_name == NULL)
	{
		temp_file_name = (char *)malloc(strlen(path) + 1);
		strcpy(temp_file_name,path);
		mktemp(temp_file_name);
	}

	if(execld(load_address, temp_file_name, filename))
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
	true_image_size = header.a_text + header.a_data + header.a_bss;

	if (true_image_size > image_size)
	{
		fclose(temp_file);
		free(load_address);
		load_address = valloc(true_image_size);

		if (execld(load_address, temp_file_name, filename))
		{
			*err = "ld failed!";
			goto finish_up;
		}
		temp_file = fopen(temp_file_name,"r");
		nread = fread(&header, sizeof(header), 1, temp_file);
	}

	fseek(temp_file, N_TXTOFF(header), 0);
	nread = fread(load_address, true_image_size,1,temp_file);

	retval = load_symbols(filename, &ld_header, load_address);

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
load_symbols(filename, hdr, entry_addr)

char *filename;
struct exec *hdr;
int entry_addr;

{
	int fd;
	char *strings, *symb_table, *p, *q;
	int symtab_offset, string_offset, string_size, nsyms, i;
	struct nlist *table_entry;
	int entering = 1;
	DynamicFunctionList *head, *scanner;

	symtab_offset = N_SYMOFF(*hdr);
	string_offset = N_STROFF(*hdr);

	fd = open(filename, O_RDONLY);

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

/* 
 *   ld -N -x -A SYMBOL_TABLE -T ADDR -o TEMPFILE FUNC -lc
 */

execld(address, tmp_file, filename)

char *address, *tmp_file, *filename;

{
	char command[256];
	int retval;

	sprintf(command,"ld -N -x -A %s -T %lx -o %s  %s -lc -lm -ll",
	    	pg_pathname,
	    	address,
	    	tmp_file,  filename);
	retval = system(command);
	return(retval);
}
