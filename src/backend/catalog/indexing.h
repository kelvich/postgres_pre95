/*
 * This include provides some definitions to support indexing on system
 * catalogs
 *
 * $Header$
 */

#ifndef _INDEXING_H_INCL_
#define _INDEXING_H_INCL_

/*
 * Some definitions for indices on pg_attribute
 */
#define Num_pg_attr_indices	2


#if 0
/*
 * Names of indices on system catalogs
 */
extern Name AttributeNameIndex;
extern Name AttributeNumIndex;

extern char *Name_pg_attr_inds[];

#endif

/*
 * What follows are lines processed by genbki.sh to create the statements
 * the bootstrap parser will turn into DefineIndex commands.
 *
 * The keyword is DEFINE_INDEX every thing after that is just like in a
 * normal specification of the 'define index' POSTQUEL command.
 */
DEFINE_INDEX(pg_attnameind on pg_attribute using btree (attname char16_ops));
DEFINE_INDEX(pg_attnumind  on pg_attribute using btree (attnum int4_ops));


#endif /* _INDEXING_H_INCL_ */
