/* ------------------------------------------
 *   FILE
 *	catalog.h
 * 
 *   DESCRIPTION
 *	prototypes for functions in lib/catalog/catalog.c
 *
 *   IDENTIFICATION
 *	$Header$
 * -------------------------------------------
 */
#ifndef LIBCATALOGINCLUDED
#define LIBCATALOGINCLUDED
/* catalog.c */
char *relpath ARGS((char relname []));
bool issystem ARGS((char relname []));
bool NameIsSystemRelationName ARGS((Name name ));
bool NameIsSharedSystemRelationName ARGS((Name relname ));
ObjectId newoid ARGS((void ));
int fillatt ARGS((int natts , AttributeTupleForm att []));
#endif
