/*
 *  This file contains all the rules that govern all version semantics.
 *
 *  At the point the version is defined, 2 physical relations are created
 *  <vname>_added and <vname>_deleted.
 *
 *  In addition, 4 rules are defined which govern the semantics of versions 
 *  w.r.t retrieves, appends, replaces and deletes.
 *
 *  $Header$
 */

#include <stdio.h>

#include "tmp/c.h"
#include "utils/rel.h"
#include "access/heapam.h"
#include "utils/log.h"
#include "nodes/pg_lisp.h"
#include "commands/version.h"

#define MAX_QUERY_LEN 1024

char rule_buf[MAX_QUERY_LEN];
static char attr_list[MAX_QUERY_LEN];

void
CreateVersion (name, bname)
     Name     name;
     Name     bname;
{
  LispValue tmp_list = LispNil;
  int notfirst = 0;
  int length = 0;
  char *attrname;
  LispValue i, temp;
  static char temp_buf[512];

   /*
   * Calls the routine ``GetAttrList'' get the list of attributes
   * from the base relation. 
   * Code is put here so that we only need to look up the attribute once for
   * both appends and replaces.
   */

  tmp_list = GetAttrList(bname);

  foreach(i, tmp_list) {
    temp = CAR(i);
    attrname = CString(temp);

    if (notfirst == 1)
      sprintf(temp_buf, ", %s = new.%s", attrname, attrname);
    else {
      sprintf(temp_buf, "%s = new.%s", attrname, attrname);
      notfirst = 1;
    }
    strcat(attr_list, temp_buf);  
  }

  length = strlen(attr_list) + 1;
  VersionCreate (name, bname);
  VersionAppend (name, bname);
  VersionDelete (name, bname);
  VersionReplace (name, bname);
  VersionRetrieve (name, bname);
  bzero(attr_list,length);

}


void
VersionCreate (vname, bname)
     Name vname;
     Name bname;

{
  static char query_buf [MAX_QUERY_LEN];

  /*
   *  Creating the dummy version relation for triggering rules.
   */
  sprintf(query_buf, "retrieve into %s ( %s.all) where 1 =2", vname,bname);
  pg_eval (query_buf);

  /* 
   * Creating the ``v_added'' relation 
   */
  sprintf (query_buf, "retrieve into %s_added (%s.all) where 1 = 2", vname, bname);
  printf ("%s\n",query_buf);
  pg_eval (query_buf);

  /* 
   * Creating the ``v_deleted'' relation. 
   */
  sprintf (query_buf, "create %s_del(DOID = oid)", vname);
  printf ("%s\n", query_buf);
  pg_eval (query_buf);

}


/*
 * Given the relation name, GetAttrList does a catalog lookup for that
 * relation and returns the list of attributes (names) for that relation.
 */

LispValue
GetAttrList(bname)
    Name bname;
{
  Relation rdesc;
  int i = 0;
  int maxattrs = 0;
  int type_id, type_len,vnum;
  LispValue attr_list = LispNil;
  rdesc = amopenr(bname);
  if (rdesc == NULL ) {
    elog(WARN,"Unable to expand all -- amopenr failed ");
    return(NULL);
  }
  maxattrs = RelationGetNumberOfAttributes(rdesc);

  for ( i = maxattrs-1 ; i > -1 ; --i ) {
    attr_list = lispCons(lispString((char *)(&rdesc->rd_att.data[i]->attname)),
			 attr_list);
/*    printf("%s\n",attrname); */
  }

  return(attr_list);
}

/*
 * This routine defines the rule governing the append semantics of
 * versions.  All tuples appended to a version gets appended to the 
 * <vname>_added relation.
 */

void
VersionAppend (vname,bname)
     Name vname,bname;
{
  sprintf(rule_buf,
	  "define rule %s_append is on append to %s do instead append \
%s_added(%s)",
	  vname, vname, vname, attr_list);

  printf("%s\n",rule_buf);
  pg_eval(rule_buf);
 
}


/*
 * This routine defines the rule governing the retrieval semantics of
 * versions.  To retrieve tuples from a version , we need to:
 *
 *      1. Retrieve all tuples in the <vname>_added relation.
 *      2. Retrieve all tuples in the base relation which are not in 
 *         the <vname>_del relation.
 */

void
VersionRetrieve(vname,bname)
    Name vname, bname;
{

  sprintf(rule_buf, 
	  "define rule %s_retrieve is on retrieve to %s do instead\n\
retrieve (%s_1.all) from %s_1 in (%s_added | %s) where %s.oid !!= \"%s_del.DOID\"",
	  vname, vname, vname, vname, vname, bname,vname,vname);

  printf("%s\n",rule_buf);

  pg_eval(rule_buf);

}

/*
 * This routine defines the rules that govern the delete semantics of 
 * versions. Two things happens when we delete a tuple from a version:
 *
 *     1. If the tuple to be deleted was added to the version *after*
 *        the version was created, then we simply delete the tuple 
 *        from the <vname>_added relation.
 *     2. If the tuple to be deleted is actually in the base relation,
 *        then we have to mark that tuple as being deleted by adding
 *        it to the <vname>_del relation.
 */

void
VersionDelete(vname,bname)
     Name vname,bname;
{

  sprintf(rule_buf,
	  "define rule %s_delete is on delete to %s do instead \n \
{ \n\
delete %s_added where current.oid = %s_added.oid\n\
append %s_del(DOID = %s.oid) where current.oid = %s.oid \n}\n",
	  vname,vname,vname,vname,vname,bname,bname);

  printf("%s\n",rule_buf);

  pg_eval(rule_buf);

}


/*
 *  This routine defines the rules that govern the update semantics
 *  of versions. To update a tuple in a version:
 *
 *      1. If the tuple is in <vname>_added, we simply ``replace''
 *         the tuple (as per postgres style).
 *      2. if the tuple is in the base relation, then two things have to
 *         happen:
 *         2.1  The tuple is marked ``deleted'' from the base relation by 
 *              adding the tuple to the <vname>_del relation. 
 *         2.2  A copy of the tuple is appended to the <vname>_added relation
 */

void
VersionReplace(vname, bname)
     Name vname,bname;
{
  sprintf(rule_buf,
	  "define rule %s_replace is on replace to %s do instead \n\
{ \n\
replace %s_added(%s) where current.oid = %s_added.oid \n\
append %s_del(DOID = %s.oid) where current.oid = %s.oid\n\
append %s_added(%s) where current.oid !!= \"%s_added.oid\" and current.oid = \
%s.oid\n }\n",
	  vname,vname,
	  vname,attr_list,vname,
	  vname,bname,bname,
	  vname,attr_list,vname,bname);

  printf("%s\n",rule_buf);

  pg_eval(rule_buf); 

}


