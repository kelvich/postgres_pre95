
/*
 *  $Header$
 */


/*
 * New dynamic loader.
 *
 * How does this work?  Glad you asked :-)
 *
 * In the DEC dynamic loader, we have to have done the following in order
 * for it to work:
 *
 * 1. Make sure that we stay near & etext (the highest base text address)
 *    so that we do not try to jump into the data area.  The data area starts
 *    by default at 0x1000000.
 *
 * 2. Make sure everything is linked with the -N option, so that "ld -A" will
 *    do the right thing.
 *
 * 3. Make sure loaded objects are compiled with "-G 0"
 *
 * 4. Make sure loaded objects are loaded ONCE AND ONLY ONCE.
 *
 * The algorithm is as follows:
 *
 * 1.  Find out how much text/data space will be required.  This is done
 *     by reading the header of the ".o" to be loaded.
 *
 * 2.  Execute the "ld -A" with a an address equal to some memory we malloc'ed.
 *     "ld -A" will do all the relocation, etc. for us.
 *
 * 3.  Using the output of "ld -A", read the text and data area into a valid
 *     text area.  (The DEC 3100 allows data to be read in the text area, but
 *     not vice versa.)
 * 
 * 4.  Determine which functions are defined by the object file we are loading,
 *     and using the address we loaded the output of "ld -A" into, find the
 *     addresses of those functions.  In object files, the symbol table (and 
 *     the output of nm) will give the offsets for functions.  Adding the
 *     function offset to the base text address gives the function's true
 *     address.  (This could also be done by reading the symbol table of the
 *     output of "ld -A", but it is so massive that this is VERY wasteful.)
 * 
 *     (In this case, we cheat rather hugely and use the output of nm because
 *     the symbol table format for MIPSEL/DS3100 is not well-documented).
 */ 

#include <stdio.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/file.h>

#include <a.out.h>
#include <symconst.h>
#include <mips/cachectl.h>

#include "utils/dynamic_loader.h"

extern char pg_pathname[];

static char *load_address = NULL;
static char *temp_file_name = NULL;
static char *path = "/usr/tmp/postgresXXXXXX";

#define PAGE_ROUND(X) ((X) % 512 == 0 ? (X) : (X) - (X) % 512 + 512)

DynamicFunctionList *
dynamic_file_load(err, filename, start_addr, size)

char **err, *filename, **start_addr;
long *size;

{
	extern etext, edata, end;
	extern char *mktemp();

	int nread;
	char command[256];
	unsigned long image_size, true_image_size;
	FILE *temp_file = NULL;
	DynamicFunctionList *retval = NULL, *load_symbols();
	struct filehdr obj_file_struct, ld_file_struct;
	AOUTHDR obj_aout_hdr, ld_aout_hdr;
	struct scnhdr scn_struct;
	int size_text, size_data = 0, size_bss = 0, bss_offset;
	int i, fd;

	fd = open(filename, O_RDONLY);

	read(fd, & obj_file_struct, sizeof(struct filehdr));
	read(fd, & obj_aout_hdr, sizeof(AOUTHDR));

	read(fd, & scn_struct, sizeof(struct scnhdr)); /* text hdr */
	size_text = scn_struct.s_size;
	if (obj_file_struct.f_nscns > 1)
	{
		read(fd, & scn_struct, sizeof(struct scnhdr)); /* data hdr */
		size_data = scn_struct.s_size;
	}

/*
 * add 10000 for fudge factor to account for data areas that appear in
 * the linking process (yes, there are such beasts!).
 */

	image_size = size_text + size_data + 10000;

	fd = open(filename, O_RDONLY);

	if (temp_file_name == NULL)
	{
		temp_file_name = (char *)malloc(strlen(path) + 1);
	}

	strcpy(temp_file_name,path);
	mktemp(temp_file_name);

	load_address = (char *) valloc(image_size);

	sprintf(command,"ld -N -A %s -T %lx -o %s  %s -lc -lm -ll",
	    pg_pathname,
	    load_address,
	    temp_file_name,  filename);

	if (system(command))
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

	fread(&scn_struct, sizeof(struct scnhdr), 1, temp_file); /* text hdr */

/*
 * Figure out how big the data areas (including the bss area) are,
 * and determine where the bss area is if there is one.
 */

	true_image_size = scn_struct.s_size;
	for (i = 1; i < ld_file_struct.f_nscns; i++)
	{
		fread(&scn_struct, sizeof(struct scnhdr), 1, temp_file);
		true_image_size += scn_struct.s_size;
		if (!strcmp(scn_struct.s_name, "bss"))
		{
			size_bss = scn_struct.s_size;
			bss_offset = scn_struct.s_vaddr - (int) load_address;
		}
	}

/*
 * Here we see if our "fudge guess" above was too small.  We do it this way
 * because loading is so ungodly expensive, and we want to avoid having to
 * create 3 megabyte files unnecessarily.
 */

	if (true_image_size > image_size)
	{
		free(load_address);
		fclose(temp_file);
		unlink(temp_file_name);
		load_address = (char *) valloc(true_image_size);
		sprintf(command,"ld -N -A %s -T %lx -o %s  %s -lc -lm -ll",
	    		pg_pathname,
	    		load_address,
	    		temp_file_name,  filename);
		system(command);
		temp_file = fopen(temp_file_name,"r");
		fread(&ld_file_struct, sizeof(struct filehdr), 1, temp_file);
		fread(&ld_aout_hdr, sizeof(AOUTHDR), 1, temp_file);
	}

	fseek(temp_file, N_TXTOFF(ld_file_struct, ld_aout_hdr), 0);

	fread(load_address, true_image_size,1,temp_file);

	/* zero the BSS segment */

	if (size_bss != 0)
	{
		bzero(bss_offset + load_address, size_bss);
	}

#ifdef NEWDEC
	if (cachectl(load_address, PAGE_ROUND(image_size), UNCACHEABLE))
	{
		*err = "dynamic_file_load: Cachectl failed!";
	}
	else
#endif
	{
		retval = load_symbols(filename, load_address);
	}

finish_up:
	fclose(temp_file);
	unlink(temp_file_name);
	*start_address = load_address;
	*size = true_image_size;
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
