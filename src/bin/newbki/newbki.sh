#!/bin/sh

# ----------------------------------------------------------------
#   FILE
#	newbki	change userid of postgres login in bki files
#
#   DESCRIPTION
#	this program changes the value of the userid associated
#	with the postgres login inside the database files.
#	It can ONLY be run before initdb(1) has been run, or if you
#	completely clear out the database directory using cleardbdir(1),
#	which, of course, destroys all the existing databases.  It is
#	useful before you start making databases, or if you first
#	copy out all your databases to ASCII files and then reload them.
#
# ----------------------------------------------------------------
[ -z "$PGDATA" ] && PGDATA=_fUnKy_DATADIR_sTuFf_


cd $PGDATA || exit

if [ -f ../pg_user]; then
echo  "Error: it looks like you already ran initdb."
echo  "You should first run cleardbdir if you want to reset the postgres userid."
exit
fi
echo -n 'Enter username that postgres should use (default is postgres):  "
read name || exit
case $name in
"")	name=postgres ;;
esac

pguid=
pguid=`pg_id $name`
case $pguid in
"")
	echo "Failed: did you put the postgres bin/ in your path?"
	exit
	;;
NOUSER)
	echo "Failed: the username $name does not exist on this system - please"
	echo "choose a login name that exists"
	exit
	;;
esac

rm -f *.bki
for i in *.source
do
	dest=`echo $i | sed 's/\.real$//'`
	sed "s/PGUID/$pguid/" < $i > $dest
done
echo "Done, you can now run initdb."
