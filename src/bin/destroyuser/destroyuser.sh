#!/bin/sh
# set -x
#
# $Header$
#
# Note - this should NOT be setuid.
#

#
# find postgres tree
#

if (test -n "$POSTGRESHOME")
then
    PG=$POSTGRESHOME
else
    PG=/usr/postgres
fi

#
# find monitor program
#

if (test -f "$PG/bin/monitor")
then
    MONITOR=$PG/bin/monitor
elif (test -f "MONITOR=$PG/obj*/support/monitor")
then
    MONITOR=$PG/obj*/support/monitor
else
    echo "$0: can't find the monitor program!"
    exit 1
fi

progname=$0

if (test -n "$PGPORT")
then
    port=$PGPORT
else
    port=4321
fi

if (test -n "$PGHOST")
then
    host=$PGHOST
else
    host=localhost
fi

while (test -n "$1")
do
    case $1 in 
        -h) host=$2; shift;;
        -p) port=$2; shift;;
         *) DELUSER=$1;;
    esac
    shift;
done

MARGS="-TN -p $port -h $host"

#
# generate the first part of the actual monitor command
#

MONITOR="$MONITOR $MARGS"

#
# see if user $USER is allowed to create new users.  Only a user who can
# create users can delete them.
#

QUERY='retrieve (pg_user.usesuper) where pg_user.usename = '"\"$USER\""
ADDUSER=`$MONITOR -c "$QUERY" template1`

if (test $? -ne 0)
then
    echo "$0: database access failed."
    exit 1
fi

if (test $ADDUSER != "t")
then
    echo "$0: $USER cannot delete users."
fi

#
# get the user name of the user to delete.  Make sure it exists.
#

if (test -z "$DELUSER")
then
    echo -n "Enter name of user to delete ---> "
    read DELUSER
fi

QUERY='retrieve (pg_user.usesysid) where pg_user.usename = '"\"$DELUSER\""

RES=`$MONITOR -c "$QUERY" template1`

if (test $? -ne 0)
then
    echo "$0: database access failed."
    exit 1
fi

if (test ! -n "$RES")
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

if (test $? -ne 0)
then
    echo "$0: database access failed - exiting..."
    exit 1
fi


#
# don't try to delete template1!
#

for i in $ALLDBS
do
    if (test $i != "template1")
    then
        DBLIST="$DBLIST $i"
    fi
done

if (test -n "$DBLIST")
then
    echo "User $DELUSER owned the following databases:"
    echo $DBLIST
    echo

#
# Now we warn the DBA that deleting this user will destroy a bunch of databases
#

    yn=f
    while (test $yn != y -a $yn != n)
    do
        echo -n "Deleting user $DELUSER will destroy them. Continue (y/n)? "
        read yn
    done

    if (test $yn = n)
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
        if (test $? -ne 0)
        then
            echo "$0: destroydb on $i failed - exiting"
            exit 1
        fi
    done
fi

QUERY="delete pg_user where pg_user.usename = \"$DELUSER\""

$MONITOR -c "$QUERY" template1
if (test $? -ne 0)
then
    echo "$0: delete of user $DELUSER was UNSUCCESSFUL"
else
    echo "$0: delete of user $DELUSER was successful."
fi

exit 0
