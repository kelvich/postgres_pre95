#!/bin/sh
# ----------------------------------------------------------------
#   FILE
#    createdb    create a postgres database
#
#   DESCRIPTION
#    this program runs the monitor with the "-c" option to create
#   the requested database.
#
#   IDENTIFICATION
#     $Header$
# ----------------------------------------------------------------

# ----------------
#       Set paths from environment or default values.
#       The _fUnKy_..._sTuFf_ gets set when the script is installed
#       from the default value for this build.
#	Currently the only thing wee look for from the environment is
#	PGDATA, PGHOST, and PGPORT
#
# ----------------
[ -z "$PGPORT" ] && PGPORT=4321
[ -z "$PGHOST" ] && PGHOST=localhost
BINDIR=_fUnKy_BINDIR_sTuFf_
PATH=$BINDIR:$PATH

dbname=$USER

while test -n "$1"
do
    case $1 in
        -h) PGHOST=$2; shift;;
        -p) PGPORT=$2; shift;;
         *) dbname=$1;;
    esac
    shift;
done

monitor -TN -h $PGHOST -p $PGPORT -c "createdb $dbname" template1 || {
    echo "$progname: database creation failed on $dbname."
    exit 1
}

exit 0
