/*
 *  FILE
 *	dynloader.c
 *
 *  DESCRIPTION
 *	dynamic loader for HP-UX using the shared library mechanism
 *
 *  INTERFACE ROUTINES
 *	dynamic_file_load
 *
 *  NOTES
 *	This code comes from RIchard Turnbull's port of 4.0.1 to 
 *	HP-UX 8.07.
 *
 *  IDENTIFICATION
 *	$Header$
 */

/* Postgres HP-UX Dynamic Loader (C) RIK Dec 1991 */

/* System includes */
#include <stdio.h>
#include <a.out.h>
#include <dl.h>

/* Postgres includes */
#include "tmp/c.h"
#include "fmgr.h"
#include "utils/dynamic_loader.h"

extern char pg_pathname[];

static char *path = "/usr/tmp/";

DynamicFunctionList *read_symbols();
void free_dyna();

DynamicFunctionList *dynamic_file_load(err, filename, libname, handle )
char **err, *filename, **libname;
shl_t *handle;
{

    FILE *temp_file = NULL;

    DynamicFunctionList *dyna_head = NULL;
    DynamicFunctionList *this_dyna = NULL;

    char pidnum[10];
    char *endptr = NULL;
    char *startptr = NULL;
    char *fname = NULL;
    char *temp_file_name = NULL;
    int str_len = 0;

    /* Build name of shared library: */

    startptr = strrchr( filename, '/' );
    startptr++;

    fname = (char *) malloc( strlen( startptr ) + 1);
    if( fname == NULL )
    {
        *err = "could not malloc memory";
        goto failed;
    }
    strcpy( fname, startptr );

    endptr = strchr( fname, '.' );
    if( endptr != NULL )
        *endptr = '\0';

    sprintf( pidnum, "%d", getpid() );
    str_len = strlen(path) + strlen(fname) + strlen(pidnum) + 5;
    if( ( temp_file_name = (char *) malloc( str_len ) ) == NULL )
    {
        *err = "could not malloc memory";
        goto failed;
    }
    sprintf( temp_file_name, "%s%s%s.sl", path, fname, pidnum );

    if( ( dyna_head = read_symbols(filename) ) == NULL )
    {
        *err = "could not read symbol table";
        goto failed;
    }

    if( execld( temp_file_name, filename ) )
    {
        *err = "link failed";
        free_dyna( &dyna_head );
        dyna_head = NULL;
        goto failed;
    }

    if( ( (*handle) = shl_load( temp_file_name, BIND_DEFERRED, NULL ) ) == 0 )
    {
        *err = "unable to load shared library";
        free_dyna( &dyna_head );
        dyna_head = NULL;
        goto failed;
    }

    this_dyna = dyna_head;

    while( this_dyna != NULL )
    {
        if( shl_findsym( handle, 
                         this_dyna->funcname, 
                         TYPE_PROCEDURE,
                         &(this_dyna->func)  ) == -1 )
        {
            *err = "unable to find symbol";
            free_dyna( &dyna_head );
            dyna_head = NULL;
            goto failed;
        }
        this_dyna = this_dyna->next;
    }

    *libname = temp_file_name;
    temp_file_name = NULL;

failed:

    if( fname != NULL )
        free( fname );

    if( temp_file_name != NULL )
       free( temp_file_name );

    return( dyna_head );

}

int execld( tmp_file, filename )
char *tmp_file, *filename;
{

    char command[256];
    int retval;

    sprintf( command, "ld -b -o %s %s" , tmp_file, filename );

    retval = system(command);

    return(retval);

}

func_ptr dynamic_load(err)
char **err;
{
    *err = "Dynamic load: Should not be here!";
    return(NULL);
}

DynamicFunctionList *read_symbols( filename )
char *filename;
{

    FILE *fp = NULL;

    int st_size  = 0;
    int str_size = 0;
    int no_syms  = 0;

    char *strings    = NULL;
    char *stable     = NULL;
    char *st_entry   = NULL;
    char *str_entry  = NULL;

    struct symbol_dictionary_record *this_entry = NULL;

    DynamicFunctionList *dyna_head = NULL;
    DynamicFunctionList *this_dyna = NULL;

    struct header fheader;

    /* Open object file: */

    if( ( fp = fopen( filename, "r" ) ) == NULL )
        goto cleanup;

    /* Read header: */

    if( fread( &fheader, sizeof(fheader), 1, fp ) <= 0 )
        goto cleanup;

    /* Get size of string and symbol table: */

    str_size = fheader.symbol_strings_size;
    st_size  = fheader.symbol_total * 
               sizeof(struct symbol_dictionary_record);

    /* Allocate memory and read in string table: */

    if( ( strings = (char *) malloc( str_size ) ) == NULL )
        goto cleanup;

    if( fseek( fp, fheader.symbol_strings_location, SEEK_SET ) )
        goto cleanup;

    if( fread( strings, str_size, 1, fp ) <= 0 )
        goto cleanup;

    /* Allocate memory for symbol table and read it in: */

    if( ( stable = (char *) malloc( st_size ) ) == NULL )
        goto cleanup;

    if( fseek( fp, fheader.symbol_location, SEEK_SET ) )
        goto cleanup;

    if( fread( stable, st_size, 1, fp ) <= 0 )
        goto cleanup;

    /* Close object file: */

    fclose(fp);
    fp = NULL;

    st_entry = stable;

    /* This is dodgy as we need to if nlist.n_name is a ptr or a  */
    /* fixed length!                                              */

    for( no_syms = 0; no_syms < fheader.symbol_total; no_syms++ )
    {

        /* Get a symbol table entry: */

        this_entry = (struct symbol_dictionary_record *) st_entry;

        /* Get a pointer to the next entry: */

        st_entry  += sizeof(struct symbol_dictionary_record);

        /* Get the name of the symbol: */

        if( this_entry->symbol_type == ST_ENTRY &&
            this_entry->symbol_scope == SS_UNIVERSAL )
        {

            /* Special case for head of list: */

            if( dyna_head == NULL )
            {
                dyna_head = (DynamicFunctionList *) 
                                malloc( sizeof( DynamicFunctionList ) );
                this_dyna = dyna_head;
            }
            else
            {
                this_dyna->next = (DynamicFunctionList *) 
                                       malloc( sizeof( DynamicFunctionList ) );
                this_dyna = this_dyna->next;
            }

            /* Check to see if we allocated memory OK: */

            if( this_dyna == NULL )
            {
                free_dyna( &dyna_head );
                goto cleanup;
            }
 
            this_dyna->next = NULL;

            /* Find name of symbol in string table: */
                
            str_entry = strings + this_entry->name.n_strx;

            /* Copy it into the linked list: */

            strcpy( this_dyna->funcname, str_entry );
        }
    }

cleanup:

    if( fp != NULL )
        fclose( fp );

    if( stable != NULL )
    	free(stable);

    if( strings != NULL )
        free(strings);

    return( dyna_head );

}

void free_dyna( dyna_list )
DynamicFunctionList **dyna_list;
{

    DynamicFunctionList *this_dyna = NULL;
    DynamicFunctionList *that_dyna = NULL;

    this_dyna = (*dyna_list );

    while( this_dyna != NULL )
    {
        that_dyna = this_dyna ->next;
        free( this_dyna );
        this_dyna = that_dyna;
    }

}
