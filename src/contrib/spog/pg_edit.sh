#! /bin/sh
#
# this script is useful for editing the contains of a relation 
# in your favorite editor. NOTE: THIS SHOULD NOT BE THE GENERAL
# TOOL FOR DATABASE UPDATES SINCE YOU CAN DESTROY THE INTEGRITY
# OF YOUR DATABASE.
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

sep=":::"

if [ $# -ne 2 ] ; then
	echo "usage: reledit relation database"
	exit 42
fi

db=$2
rel=$1
tmp=/tmp/$rel.$$

# check the database
err=`spog -n -c "retrieve (pg_version.all)" $db | grep FATAL`
if [ "X$err" != "X" ] ; then
        echo "No such database $db."
        echo $err
        exit 42
fi

# check the relation
err=`spog -n -c "retrieve ($rel.all) where 1=0" $db | grep failed`
if [ "X$err" != "X" ] ; then
        echo "No such relation $rel."
        echo $err
        exit 42
fi

# get the attributes of this relation (remove system attributes)
attnames=`(
  echo "format attribute attname \"%s\""
  echo "format attribute typname \"::%s\""
  echo "retrieve (a.attname, t.typname) from a in pg_attribute, t in pg_type, r in pg_class where a.attrelid = r.oid and a.atttypid = t.oid and r.relname = \"$rel\""
) | spog -n -f - $db `
attnames=`echo $attnames| awk '{ for (i=1;i<=NF-12;i++) print $i }'`

if [ $? -ne 0 ] ; then
	echo "cant open $db"
	exit 42
fi

(
echo "format global \"$sep%s\""
##echo "format type oid \"%s\""
echo "retrieve ($rel.oid,$rel.all)"
) | spog -n -f - $db > $tmp

cp $tmp $tmp.new
vi $tmp.new
echo $attnames > $tmp.newlines
diff $tmp $tmp.new | grep "^>" >> $tmp.newlines
rm -f $tmp $tmp.new

gawk ' \
	NF==1	{ split($0,attnames,/ /)}; \
	NF>1	{ gsub(/> /,""); \
		  printf("replace %s(", rel); \
		  for (i in attnames) { \
		    split(attnames[i],aa,/::/) \
		    name = aa[1]; type = aa[2]; \
		    if (i > 1 ) printf (","); \
		    printf ("%s=\"%s\"::%s",name, $(i+2), type); \
		  } \
		  printf (") where %s.oid = %s::oid\n", rel, $2); \
		}' FS=$sep rel=$rel $tmp.newlines | spog -n -f - $db

rm -f $tmp.newlines

