/*
 * Interface to Postgres 2.0
 *
 * Tue Aug 21 14:13:40 1990  Igor Metz <metz@iam.unibe.ch>
 *
 * $Header$
 * $Log$
 * Revision 1.1  1990/10/24 20:31:25  cimarron
 * Initial revision
 *
 * Revision 1.1  90/08/23  11:49:47  metz
 * Initial revision
 * 
 */

#include "EXTERN.h"
#include "perl.h"

extern void init_postgres_stuff();

int
userinit()
{
  init_postgres_stuff();
  return 0;
}

