#!/bin/sh
#
# $Header$
#
# Note - this should NOT be setuid.
#

# ----------------
#       Set paths from environment or default values.
#       The _fUnKy_..._sTuFf_ gets set when the script is installed
#       from the default value for this build.
#       Currently the only thing wee look for from the environment is
#       PGDATA, PGHOST, and PGPORT
#
# ----------------
[ -z "$PGPORT" ] && PGPORT=4321
[ -z "$PGHOST" ] && PGHOST=localhost
BINDIR=_fUnKy_BINDIR_sTuFf_
PATH=$BINDIR:$PATH

while (test -n "$1")
do
    case $1 in 
	-a) AUTHSYS=$2; shift;;
        -h) PGHOST=$2; shift;;
        -p) PGPORT=$2; shift;;
         *) DELUSER=$1;;
    esac
    shift;
done

AUTHOPT="-a $AUTHSYS"
[ -z "$AUTHSYS" ] && AUTHOPT=""

MARGS="-TN $AUTHOPT -p $PGPORT -h $PGHOST"

#
# generate the first part of the actual monitor command
#
MONITOR="monitor $MARGS"

#
# see if user $USER is allowed to create new users.  Only a user who can
# create users can delete them.
#

QUERY='retrieve (pg_user.usesuper) where pg_user.usename = '"\"$USER\""
ADDUSER=`$MONITOR -c "$QUERY" template1`

if [ $? -ne 0 ]
then
    echo "$0: database access failed."
    exit 1
fi

if [ $ADDUSER != "t" ]
then
    echo "$0: $USER cannot delete users."
fi

#
# get the user name of the user to delete.  Make sure it exists.
#

if [ -z "$DELUSER" ]
then
    echo _fUnKy_DASH_N_sTuFf_ "Enter name of user to delete ---> "_fUnKy_BACKSLASH_C_sTuFf_
    read DELUSER
fi

QUERY='retrieve (pg_user.usesysid) where pg_user.usename = '"\"$DELUSER\""

RES=`$MONITOR -c "$QUERY" template1`

if [ $? -ne 0 ]
then
    echo "$0: database access failed."
    exit 1
fi

if [ ! -n "$RES" ]
then
    echo "$0: user "\"$DELUSER\"" does not exist."
    exit 1
fi

SYSID=$RES

#
# destroy the databases owned by the deleted user.  First, use this query
# to find out what they are.
#

QUERY="retrieve (pg_database.datname) where \
       pg_database.datdba = \"$SYSID\"::oid"

ALLDBS=`$MONITOR -c "$QUERY" template1`

if [ $? -ne 0 ]
then
    echo "$0: database access failed - exiting..."
    exit 1
fi


#
# don't try to delete template1!
#

for i in $ALLDBS
do
    if [ $i != "template1" ]
    then
        DBLIST="$DBLIST $i"
    fi
done

if [ -n "$DBLIST" ]
then
    echo "User $DELUSER owned the following databases:"
    echo $DBLIST
    echo

#
# Now we warn the DBA that deleting this user will destroy a bunch of databases
#

    yn=f
    while [ $yn != y -a $yn != n ]
    do
        echo _fUnKy_DASH_N_sTuFf_ "Deleting user $DELUSER will destroy them. Continue (y/n)? "_fUnKy_BACKSLASH_C_sTuFf_
        read yn
    done

    if [ $yn = n ]
    then
        echo "$0: exiting"
        exit 1
    fi

    #
    # now actually destroy the databases
    #

    for i in $DBLIST
    do
        echo "destroying database $i"

        QUERY="destroydb $i"
        $MONITOR -c "$QUERY" template1
        if [ $? -ne 0 ]
        then
            echo "$0: destroydb on $i failed - exiting"
            exit 1
        fi
    done
fi

QUERY="delete pg_user where pg_user.usename = \"$DELUSER\""

$MONITOR -c "$QUERY" template1
if [ $? -ne 0 ]
then
    echo "$0: delete of user $DELUSER was UNSUCCESSFUL"
else
    echo "$0: delete of user $DELUSER was successful."
fi

exit 0
