
/*
 *  $Header$
 */

#include <pwd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/file.h>

#include <a.out.h>
#include <symconst.h>

#include "utils/dynamic_loader.h"

extern char pg_pathname[];

/* 
 *
 *   ld -N -x -A SYMBOL_TABLE -o TEMPFILE FUNC -lc
 *
 *   ld -N -x -A SYMBOL_TABLE -T ADDR -o TEMPFILE FUNC -lc
 *
 */

/*
 * This is initially set to the value of etext;
 */

static char *load_address = NULL;
static char *temp_file_name = NULL;
static char *path = "/usr/tmp/postgresXXXXXX";

DynamicFunctionList *
dynamic_file_load(err, filename)

char **err, *filename;

{
	extern etext, edata, end;
	extern char *mktemp();

	int nread;
	unsigned long image_size, zz;
	FILE *temp_file = NULL;
	char command[256];
	DynamicFunctionList *retval = NULL, *load_symbols();
	int fd;
	struct filehdr obj_file_struct, ld_file_struct;
	AOUTHDR obj_aout_hdr, ld_aout_hdr;
	struct scnhdr scn_struct;
	int size_text, size_data, size_bss;

/*
 * Make sure (when compiling) that there is sufficient space for loading
 * all functions!
 */

	if (load_address == NULL)
	{
		load_address = (char *) &etext;
	}

	fd = open(filename, O_RDONLY);

	read(fd, & obj_file_struct, sizeof(struct filehdr));
	read(fd, & obj_aout_hdr, sizeof(AOUTHDR));

	read(fd, & scn_struct, sizeof(struct scnhdr)); /* text hdr */
	size_text = scn_struct.s_size;
	read(fd, & scn_struct, sizeof(struct scnhdr)); /* data hdr */
	size_data = scn_struct.s_size;

	image_size = size_text + size_data;

	fd = open(filename, O_RDONLY);

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
	fread(&ld_file_struct, sizeof(struct filehdr), 1, temp_file);
	fread(&ld_aout_hdr, sizeof(AOUTHDR), 1, temp_file);

	if (ld_file_struct.f_nscns > 2) /* Need to read BSS segment */
	{
		fread(&scn_struct, sizeof(struct scnhdr), 1, temp_file); /* text hdr */
		fread(&scn_struct, sizeof(struct scnhdr), 1, temp_file); /* data hdr */
		fread(&scn_struct, sizeof(struct scnhdr), 1, temp_file); /* BSS hdr */
		size_bss = scn_struct.s_size;
	}

	fseek(temp_file, N_TXTOFF(ld_file_struct, ld_aout_hdr), 0);

	fread(load_address, image_size,1,temp_file);

	/* zero the BSS segment */

	if (size_bss != 0)
	{
		bzero(load_address + image_size - size_bss, size_bss);
	}

	retval = load_symbols(filename, load_address);

	fclose(temp_file);
	unlink(temp_file_name);

	temp_file = NULL;

 /*
  * Need to make sure we have enough free text addresses to load lots of
  * files.
  */

	load_address += image_size;
			   
finish_up:
	if (temp_file != NULL) fclose(temp_file);
	return retval;
}

/*
 * Cheat massively because I can't figure out how to read the symbol table
 * properly, so use system("nm ...") to do it instead.
 */

DynamicFunctionList *
load_symbols(filename, entry_addr)

char *filename;
int entry_addr;

{
	char command[256];
	char line[128];
	char *tmp_file = "/tmp/PG_DYNSTUFF";
	FILE *fp;
	DynamicFunctionList *head, *scanner;
	int entering = 1, func_addr;
	char funcname[16];

	sprintf(command, "/usr/bin/nm %s | grep \' T \' > %s", filename, tmp_file);

	if (system(command))
	{
		fprintf(stderr, "system() died\n");
	}

	fp = fopen(tmp_file, "r");

	while (fgets(line, 127, fp) != NULL)
	{
		sscanf(line, "%lx T %s", &func_addr, funcname);
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

		strcpy(scanner->funcname, funcname);
		scanner->func = (func_ptr) (func_addr + entry_addr);
		scanner->next = NULL;
	}

	unlink(tmp_file);
	return(head);
}

func_ptr
dynamic_load(err)

char **err;

{
	*err = "Dynamic load: Should not be here!";
	return(NULL);
}

remove() {}
