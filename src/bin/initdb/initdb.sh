#!/bin/sh
# ----------------------------------------------------------------
#   FILE
#	initdb	create a postgres template database
#
#   DESCRIPTION
#	this program feeds the proper input to the ``backend'' program
#	to create a postgres database and register it in the
#	shared ``pg_database'' database.
#
#   IDENTIFICATION
# 	$Header$
# ----------------------------------------------------------------

CMDNAME=`basename $0`

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
if (test "$debug" -eq 1)
then
    BACKENDARGS="-COd -ami"
else
    BACKENDARGS="-COQ -ami"
fi

# ----------------
# 	check POSTGRESHOME
# ----------------
if (test -z "$POSTGRESHOME")
then
    echo "$CMDNAME: POSTGRESHOME not set."
    exit 1
else
    PG=`echo $POSTGRESHOME | sed -e 's/\([^:]*\):.*/\1/'`
    PGS=`echo $POSTGRESHOME | sed -e 's/:/ /g'`
fi

# ----------------
# 	check POSTGRESTREE
# ----------------
if (test -z "$POSTGRESTREE")
then
    TREE=$PG
else
    TREE=$POSTGRESTREE
fi

# ----------------
# 	check POSTGRESBIN
# ----------------
if (test -z "$POSTGRESBIN")
then
    PGBIN=$PG/bin
else
    PGBIN=$POSTGRESBIN
fi

# ----------------
# 	check POSTGRESTEMP
# ----------------
if (test -z "$POSTGRESTEMP")
then
    PGTEMP=$PG/files
else
    PGTEMP=$POSTGRESTEMP
fi

# ----------------
# 	find the paths to the backend, pg_version, and pg_id programs
# ----------------
if (test "$verbose" -eq 1)
then
    echo "$CMDNAME: looking for backend..."
fi

if (test -f $PGBIN/backend)
then
    BACKEND=$PGBIN/backend
    PG_VERSION=$PGBIN/pg_version
	PG_ID=$PGBIN/pg_id
    if (test "$verbose" -eq 1)
    then
        echo "$CMDNAME: found $BACKEND"
    fi
elif (test -f $TREE/*/support/backend)
then
    BACKEND=$TREE/*/support/backend
    PG_VERSION=$TREE/*/support/pg_version
	PG_ID=$TREE/*/support/pg_id
    if (test "$verbose" -eq 1)
    then
        echo "$CMDNAME: found $BACKEND"
    fi
else
    echo "$CMDNAME: could not find backend program"
    echo "$CMDNAME: set POSTGRESHOME to the proper directory and rerun."
    exit 1
fi

# ----------------
# 	find the template files
# ----------------

if (test "$verbose" -eq 1)
then
    echo "$CMDNAME: looking for template files..."
fi

if (test -f $PGTEMP/local1_template1.bki)
then
    TEMPLATE=$PGTEMP/local1_template1.bki
    GLOBAL=$PGTEMP/global1.bki
else
    echo "$CMDNAME: warning: Make install has not been run"
    TEMPLATE=`echo $TREE/*/support/local.bki`
    GLOBAL=`echo $TREE/*/support/dbdb.bki`

    if (test ! -f $TEMPLATE)
    then
        echo "$CMDNAME: could not find template files"
        echo "$CMDNAME: set POSTGRESTEMP to the proper directory and rerun."
        exit 1
    fi
fi

if (test "$verbose" -eq 1)
then
    echo "$CMDNAME: using $TEMPLATE"
    echo "$CMDNAME: using $GLOBAL"
fi

#
# Figure out who I am...
#

UID=`$PG_ID`

# ----------------
# 	create the template database if necessary
# ----------------

if (test -f "$pg/data")
then
	echo "$CMDNAME: $pg/data exists - delete it if necessary"
	exit 1
fi

for pg in $PGS
do
    mkdir $pg/data $pg/data/base $pg/data/base/template1
done

if (test "$verbose" -eq 1)
then
    echo "$CMDNAME: creating SHARED relations in $PG/data"
    echo "$CMDNAME: creating template database in $PG/data/base/template1"
fi

$BACKEND $BACKENDARGS template1 < $TEMPLATE 

if (test $? -ne 0)
then
    echo "$CMDNAME: could not create template database"
    if (test $noclean -eq 0)
    then
	    echo "$CMDNAME: cleaning up."
	    for pg in $PGS
	    do
                rm -rf $pg/data
	    done
        else
	    echo "$CMDNAME: cleanup not done (noclean mode set)."
    fi
	exit 1;
fi

$PG_VERSION $PG/data/base/template1

#
# Add the template database to pg_database
#

echo "open pg_database" > /tmp/create.$$
echo "insert (template1 $UID template1)" >> /tmp/create.$$
echo "show" >> /tmp/create.$$
echo "close pg_database" >> /tmp/create.$$

$BACKEND $BACKENDARGS template1 < $GLOBAL 

if (test $? -ne 0)
then
    echo "$CMDNAME: could create shared relations"
    if (test $noclean -eq 0)
    then
	    echo "$CMDNAME: cleaning up."
	    for pg in $PGS
	    do
            rm -rf $pg/data
	    done
    else
	    echo "$CMDNAME: cleanup not done (noclean mode set)."
    fi
	exit 1;
fi

$PG_VERSION $PG/data

$BACKEND $BACKENDARGS template1 < /tmp/create.$$ 

if (test $? -ne 0)
then
    echo "$CMDNAME: could not log template database"
    if (test $noclean -eq 0)
    then
	    echo "$CMDNAME: cleaning up."
	    for pg in $PGS
	    do
            rm -rf $pg/data
	    done
    else
	    echo "$CMDNAME: cleanup not done (noclean mode set)."
    fi
	exit 1;
fi

rm -f /tmp/create.$$
