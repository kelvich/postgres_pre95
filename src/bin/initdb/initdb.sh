#!/bin/sh

# ----------------------------------------------------------------
#   FILE
#	initdb	create a postgres template database
#
#   DESCRIPTION
#	this program feeds the proper input to the ``postgres'' program
#	to create a postgres database and register it in the
#	shared ``pg_database'' database.
#
#   IDENTIFICATION
# 	$Header$
# ----------------------------------------------------------------

CMDNAME=`basename $0`

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

# ----------------
# 	check arguments:
# 	    -d indicates debug mode.
#	    -n means don't clean up on error (so your cores don't go away)
# ----------------
debug=0
noclean=0
verbose=0

for ARG
do
	case "$ARG" in
	-d)	debug=1; echo "$CMDNAME: debug mode on";;
	-n)	noclean=1; echo "$CMDNAME: noclean mode on";;
	-v) verbose=1; echo "$CMDNAME: verbose mode on";;
	esac
done

# ----------------
# 	if the debug flag is set, then 
# ----------------
if test "$debug" -eq 1
then
    BACKENDARGS="-boot -COd -ami"
else
    BACKENDARGS="-boot -COQ -ami"
fi


TEMPLATE=$FILESDIR/local1_template1.bki
GLOBAL=$FILESDIR/global1.bki
if [ ! -f $TEMPLATE -o ! -f $GLOBAL ]
then
    echo "$CMDNAME: warning: $POSTGRESTEMP files missing"
    echo "$CMDNAME: warning: bmake install has not been run"
fi

if test "$verbose" -eq 1
then
    echo "$CMDNAME: using $TEMPLATE"
    echo "$CMDNAME: using $GLOBAL"
fi

#
# Figure out who I am...
#

UID=`pg_id`

# ----------------
# 	create the template database if necessary
# ----------------

if test -f "$PGDATA"
then
	echo "$CMDNAME: $PGDATA exists - delete it if necessary"
	exit 1
fi

mkdir $PGDATA $PGDATA/base $PGDATA/base/template1

if test "$verbose" -eq 1
then
    echo "$CMDNAME: creating SHARED relations in $PG/data"
    echo "$CMDNAME: creating template database in $PG/data/base/template1"
fi

postgres $BACKENDARGS template1 < $TEMPLATE 

if test $? -ne 0
then
    echo "$CMDNAME: could not create template database"
    if (test $noclean -eq 0)
    then
	    echo "$CMDNAME: cleaning up."
            rm -rf $PGDATA
        else
	    echo "$CMDNAME: cleanup not done (noclean mode set)."
    fi
	exit 1;
fi

pg_version $PGDATA/base/template1

#
# Add the template database to pg_database
#

echo "open pg_database" > /tmp/create.$$
echo "insert (template1 $UID template1)" >> /tmp/create.$$
echo "show" >> /tmp/create.$$
echo "close pg_database" >> /tmp/create.$$

postgres $BACKENDARGS template1 < $GLOBAL 

if (test $? -ne 0)
then
    echo "$CMDNAME: could create shared relations"
    if (test $noclean -eq 0)
    then
	    echo "$CMDNAME: cleaning up."
            rm -rf $PGDATA
    else
	    echo "$CMDNAME: cleanup not done (noclean mode set)."
    fi
	exit 1;
fi

pg_version $PGDATA

postgres $BACKENDARGS template1 < /tmp/create.$$ 

if test $? -ne 0
then
    echo "$CMDNAME: could not log template database"
    if (test $noclean -eq 0)
    then
	    echo "$CMDNAME: cleaning up."
            rm -rf $PGDATA
    else
	    echo "$CMDNAME: cleanup not done (noclean mode set)."
    fi
	exit 1;
fi

rm -f /tmp/create.$$
