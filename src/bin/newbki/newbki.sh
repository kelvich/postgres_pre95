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

CMDNAME=`basename $0`

cd $PGDATA/files || exit

if [ -d ../base ]; then
    echo "$CMDNAME: error: the following database directory already exists:"
    echo "$CMDNAME:   $PGDATA/base"
    echo "$CMDNAME: You must first run cleardbdir if you want to reset the"
    echo "$CMDNAME: postgres user id."
    exit
fi

echo _fUnKy_DASH_N_sTuFf_ 'Enter username that postgres should use (default is postgres): '_fUnKy_BACKSLASH_C_sTuFf_
read name || exit
case $name in
"")	name=postgres ;;
esac

pguid=
pguid=`pg_id $name`
case $pguid in
"")
	echo "$CMDNAME: Failed: did you put the postgres bin/ in your path?"
	exit
	;;
NOUSER)
	echo "$CMDNAME: Failed: the username $name does not exist on this"
	echo "$CMDNAME: system - please choose a login name that exists."
	exit
	;;
esac

rm -f *.bki
for i in *.source
do
	dest=`echo $i | sed 's/\.source$//'`
	sed -e "s/postgres PGUID/$name $pguid/" < $i > $dest
done
echo "Done.  You must now run initdb."
