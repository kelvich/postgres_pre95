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
#	Currently the only thing we look for from the environment is
#	PGDATA, PGHOST, and PGPORT
#
# ----------------
[ -z "$PGPORT" ] && PGPORT=4321
[ -z "$PGHOST" ] && PGHOST=localhost
BINDIR=_fUnKy_BINDIR_sTuFf_
PATH=$BINDIR:$PATH

CMDNAME=`basename $0`

if [ -z "$USER" ]; then
    if [ -z "$LOGNAME" ]; then
	if [ -z "`whoami`" ]; then
	    echo "$CMDNAME: cannot determine user name"
	    exit 1
	fi
    else
	USER=$LOGNAME
	export USER
    fi
fi

dbname=$USER

while test -n "$1"
do
    case $1 in
	-a) AUTHSYS=$2; shift;;
        -h) PGHOST=$2; shift;;
        -p) PGPORT=$2; shift;;
         *) dbname=$1;;
    esac
    shift;
done

AUTHOPT="-a $AUTHSYS"
[ -z "$AUTHSYS" ] && AUTHOPT=""

monitor -TN $AUTHOPT -h $PGHOST -p $PGPORT -c "createdb $dbname" template1 || {
    echo "$CMDNAME: database creation failed on $dbname."
    exit 1
}

exit 0
