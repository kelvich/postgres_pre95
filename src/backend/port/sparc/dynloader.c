
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
#ifndef MAXPATHLEN
# define MAXPATHLEN 1024
#endif

extern char pg_pathname[];

#include <a.out.h>

#include "utils/dynamic_loader.h"

static char partial_symbol_table[50] = "";

/* 
 *
 *   ld -N -x -A SYMBOL_TABLE -o TEMPFILE FUNC -lc
 *
 *   ld -N -x -A SYMBOL_TABLE -T ADDR -o TEMPFILE FUNC -lc
 *
 */

func_ptr
dynamic_load(err, filename, funcname)
char *filename, *funcname;
int err;
{
	extern end;
	extern char *mktemp();
	extern char *valloc();

	char *path;
	int nread;
	struct exec header;
	unsigned long image_size, zz;
	char *load_address = NULL;
	char *temp_file_name = NULL;
	FILE *temp_file = NULL;
	char command[5000];

	if (!strlen(partial_symbol_table))
		strcpy(partial_symbol_table,pg_pathname);
	path = "/usr/tmp/postgresXXXXXX";
	temp_file_name = (char *)malloc(strlen(path) + 1);
	strcpy(temp_file_name,path);
	mktemp(temp_file_name);

/* commented out -lc */

	sprintf(command,"ld -N  -A %s -o  %s %s ",
	    partial_symbol_table,
	    temp_file_name, filename);

	printf("command=%s\n",command);

	if(system(command))
	{
		fprintf(stderr,"{ERROR: link failed: %s}\n", command);
		goto finish_up;
	}
	printf("command 1 executed\n");

	if(!(temp_file = fopen(temp_file_name,"r")))
	{
		fprintf(stderr,"{ERROR: unable to open %s}\n", temp_file_name);
		goto finish_up;
	}
	nread = fread(&header, sizeof(header), 1, temp_file);
	if (nread != 1)
	{
		fprintf(stderr,"{ERROR: cant read header}\n");
		goto finish_up;
	}
		
	image_size = header.a_text + header.a_data + header.a_bss;
	fclose(temp_file);
	temp_file = NULL;

	if (!(load_address = valloc(zz=image_size))
	    )
	{
		fprintf(stderr,"{ERROR: unable to allocate memory}\n");
		goto finish_up;
	}

/* commented out -lc */

	sprintf(command,"ld -N  -A %s -T %lx -o %s  %s ",
	    partial_symbol_table,
	    load_address,
	    temp_file_name,  filename);

	printf("command 2=%s\n",command);
	if(system(command))
	{
		fprintf(stderr,"{ERROR: link failed: %s}\n", command);
		goto finish_up;
	}
	printf("command2 executed\n");
	if(!(temp_file = fopen(temp_file_name,"r")))
	{
		fprintf(stderr,"{ERROR: unable to open %s}\n", temp_file_name);
		goto finish_up;
	}
	nread = fread(&header, sizeof(header), 1, temp_file);
	if (nread != 1) 
	{
		fprintf(stderr,"{ERROR: cant read header}\n");
		goto finish_up;
	}
	image_size = header.a_text + header.a_data + header.a_bss;
	if (zz<image_size)
	{
		fprintf(stderr,"{ERROR}\n");
		goto finish_up;
	}

	fseek(temp_file, N_TXTOFF(header), 0);
	nread = fread(load_address, zz=(header.a_text + header.a_data),1,temp_file);
	if (nread != 1)
	{
		fprintf(stderr,"{ERROR: cant read file}\n");
		goto finish_up;
	}
	/* zero the BSS segment */
	while (zz<image_size)
		load_address[zz++] = 0;

	fclose(temp_file);
	temp_file = NULL;
	load_address = NULL;

	/* Prepare for future loads wrt. this one. */
	strcpy(partial_symbol_table,temp_file_name);

finish_up:
	if (temp_file != NULL) fclose(temp_file);
	if (load_address != NULL) free(load_address);
	return (func_ptr)header.a_entry;
}





/*
Here is tst.c (note that it has initialized and uninitialized variables,
and even prints something out):
 */

/*
#include <stdio.h>
int tst(x)
int x;
{
int u,v;
int z=1;
printf("This was printed in the routine. par=%d\n",x);
return 2*x;
}
 */

/*


You probably won't have any trouble converting this into a dynamic
loader program for postgres (I will work on that myself if I can find
the time).

*/

foob()

{
	printf("foob: got here!!!!!!!!!\n");
}
