#! /bin/sh
#
# This script tries to collect information about a given database.
#
# $Id$
#
# $Log$
# Revision 1.1  1993/02/18 23:29:29  aoki
# Initial revision
#
# Revision 1.1  1992/08/13  11:43:01  schoenw
# Initial revision
#
#

if [ $# -ne 1 ] ; then
  echo "usage: relinfo database"
  exit 42
fi

db=$1

# check the database
err=`spog -n -c "retrieve (pg_version.all)" $db | grep FATAL`
if [ "X$err" != "X" ] ; then
  echo "No such database $db."
  echo $err
  exit 42
fi

# get the types in this database
echo "DATATYPES:"
echo
(
  echo "separator \"\""
  echo "format global \"%-16s\""
  echo -n 'retrieve (pg_type.typname) '
  echo -n 'where pg_type.typname !~ "^_" and '
  echo -n '      pg_type.typname !~ "^pg_" and '
  echo -n '      pg_type.typname !~ "," and '
  echo    '      pg_type.typrelid=0::oid'
) | spog -n -f - $db
echo

# get the relations defined
(
  echo "format attribute attname \" %s\""
  echo "format attribute typname \"::%s \""
  echo -n 'retrieve (r.relname, a.attname, t.typname) '
  echo -n 'from r in pg_class, a in pg_attribute, t in pg_type '
  echo -n 'where a.attrelid = r.oid and a.atttypid = t.oid and '
  echo -n '      r.relname !~ "^pg_" and '
  echo -n '      r.relname !~ ","    and '
  echo -n '      r.relkind != "i"    and '
  echo -n '      a.attnum  > 0 '
  echo    'sort by relname' 
) | spog -n -f - $db | gawk \
'END 	{ print " )" } \
	{ if (NR == 1) { printf("\nRELATIONS:\n\n"); } \
	  if (last != $1) { \
	    if (NR > 1) print " )"; \
	    printf("%s (", $1); \
	    last = $1; \
	  }; \
	  printf(" %s", $2); \
	}' 

# get the indexes defined
(
  echo "format global \"%-18s\""
  echo -n 'retrieve (c1.relname,c2.relname) '
  echo -n 'from c1 in pg_class, c2 in pg_class '
  echo -n 'where c1.oid = pg_index.indexrelid and '
  echo -n '      c2.oid = pg_index.indrelid and '
  echo    '      c1.relname !~ "^pg_"'
) | spog -n -f - $db | gawk \
'	{ if (NR == 1) { printf("\nINDEXES:\n\n"); } \
	  printf("%s on relation %s\n", $1, $2) \
	}'
# get the rules 
(
  echo "retrieve (pg_prs2rule.prs2text)"
) | spog -n -f - $db | gawk \
'       { if (NR == 1) { printf("\nRULES:\n\n"); } \
	  print; \
	}'
