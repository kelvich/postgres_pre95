#!/bin/sh

# ----------------------------------------------------------------
#   FILE
#	cleardbdir	completely clear out the database directory
#
#   DESCRIPTION
#	this program clears out the database directory, but leaves the .bki
#	files so that initdb(1) can be run again.
#
# ----------------------------------------------------------------
[ -z "$PGDATA" ] && PGDATA=_fUnKy_DATADIR_sTuFf_

echo "This program completely destroys all the databases in the directory"
echo "$PGDATA"
echo _fUnKy_DASH_N_sTuFf_ "Are you sure you want to do this (y/n) [n]? "_fUnKy_BACKSLASH_C_sTuFf_
read resp || exit
case $resp in
	y*)	: ;;
	*)	exit ;;
esac

cd $PGDATA || exit
for i in *
do
if [ $i != "files" ]
then
	/bin/rm -rf $i
fi
done
