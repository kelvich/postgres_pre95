#!/bin/sh

# ----------------------------------------------------------------
#   FILE
#	reindexdb	rebuild the system catalog indices
#
#   DESCRIPTION
#	this program feeds the proper input to the ``postgres'' program
#	to drop and recreate damaged system catalog indices.  this cannot
#	be done in normal processing mode, which depends on the indices.
#
#   IDENTIFICATION
# 	$Header$
# ----------------------------------------------------------------

# ----------------
#       Set paths from environment or default values.
#       The _fUnKy_..._sTuFf_ gets set when the script is installed
#       from the default value for this build.
#       Currently the only thing wee look for from the environment is
#       PGDATA, PGHOST, and PGPORT
#
# ----------------
[ -z "$PGDATA" ] && PGDATA=_fUnKy_DATADIR_sTuFf_
[ -z "$PGPORT" ] && PGPORT=4321
[ -z "$PGHOST" ] && PGHOST=localhost
POSTGRESDIR=_fUnKy_POSTGRESDIR_sTuFf_
BINDIR=_fUnKy_BINDIR_sTuFf_
FILESDIR=$PGDATA/files
PATH=$BINDIR:$PATH

CMDNAME=`basename $0`

TMPFILE=/tmp/rebuild.$$

DBNAME=$1
if [ -z "$DBNAME" ]; then
    echo "Usage: $CMDNAME database"
    exit 1
fi

INTERNALFILE=$PGDATA/base/$DBNAME/pg_internal.init

TEMPLATE=$FILESDIR/local1_template1.bki
if [ ! -f $TEMPLATE ]; then
    echo "$CMDNAME: error: database initialization files not found."
    echo "$CMDNAME: Either bmake install has not been run or you're trying to"
    echo "$CMDNAME: run this program on a machine that does not store the"
    echo "$CMDNAME: database (PGHOST doesn't work for this)."
    exit 1
fi

echo "$CMDNAME: This command will destroy and attempt to rebuild the system"
echo "$CMDNAME: catalog indices for the database stored in the directory"
echo "$CMDNAME: $PGDATA/base/$DBNAME."
echo _fUnKy_DASH_N_sTuFf_ "Are you sure you want to do this (y/n) [n]? "_fUnKy_BACKSLASH_C_sTuFf_
read resp || exit 0
case $resp in
   y*)	: ;;
   *)	exit 0;;
esac

echo "$CMDNAME: WARNING!  You MUST kill the postmaster daemon running on"
echo "$CMDNAME: this database before attempting to rebuild catalogs!"
echo _fUnKy_DASH_N_sTuFf_ "Have you done this yet (y/n) [n]? "_fUnKy_BACKSLASH_C_sTuFf_
read resp || exit 0
case $resp in
   y*)	: ;;
   *)	exit 0;;
esac

echo "$CMDNAME: Working..."

if [ -f $INTERNALFILE ]; then
    /bin/rm $INTERNALFILE

    if [ $? -ne 0 ]; then
	echo "$CMDNAME: Attempted to remove pg_internal.init and failed."
	echo "$CMDNAME: Are you logged in as the postgres superuser?"
	exit 1
    fi
fi

echo "destroy indices" > $TMPFILE
egrep '(declare|build) ind' $TEMPLATE >> $TMPFILE

postgres -boot -COQ -ami $DBNAME < $TMPFILE

if [ $? -ne 0 ]; then
    echo "$CMDNAME: Index rebuild failed.  Your .bki files or the base"
    echo "$CMDNAME: system catalog tables may be damaged."
    exit 1
fi

rm -f $TMPFILE

echo "$CMDNAME: Done."

exit 0
