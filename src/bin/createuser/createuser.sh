#!/bin/sh
# set -x
#
# $Header$
#
# Note - this should NOT be setuid.
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
         *) NEWUSER=$1;;
    esac
    shift;
done

MARGS="-TN -h $host -p $port"

#
# generate the first part of the actual monitor command
#

MONITOR="$MONITOR $MARGS"

#
# see if user $USER is allowed to create new users
#

QUERY='retrieve (pg_user.usesuper) where pg_user.usename = '"\"$USER\""

ADDUSER=`$MONITOR -TN -c "$QUERY" template1`

if (test $? -ne 0)
then
    echo "$0: database access failed."
    exit 1
fi

if (test $ADDUSER != "t")
then
    echo "$0: $USER cannot create users."
    exit 1
fi

#
# get the user name of the new user.  Make sure it doesn't already exist.
#

if (test -z "$NEWUSER")
then
    echo -n "Enter name of user to add ---> "
    read NEWUSER
fi

QUERY='retrieve (pg_user.usesysid) where pg_user.usename = '"\"$NEWUSER\""

RES=`$MONITOR -TN -c "$QUERY" template1`

if (test $? -ne 0)
then
    echo "$0: database access failed."
    exit 1
fi

if (test -n "$RES")
then
    echo "$0: user "\"$NEWUSER\"" already exists"
    exit 1
fi

done=0

#
# get the system id of the new user.  Make sure it is unique.
#

while (test $done -ne 1)
do
    echo -n "Enter the user's Postgres system id ---> "
    read SYSID

    QUERY='retrieve (pg_user.usename) where pg_user.usesysid = '"\"$SYSID\""
    RES=`$MONITOR -TN -c "$QUERY" template1`

    if (test $? -ne 0)
    then
        echo "$0: database access failed."
        exit 1
    fi

    if (test -n "$RES")
    then
        echo 
        echo "$0: $SYSID already belongs to $RES"
        echo "The system ID must be unique."
    else
        done=1
    fi
done

#
# get the rest of the user info...
#

#
# can the user create databases?
#

yn=f

while (test "$yn" != y -a "$yn" != n)
do
    echo -n "Is user \"$NEWUSER\" allowed to create databases (y/n) "
    read yn
done

if (test "$yn" = y)
then
    CANCREATE=t
else
    CANCREATE=f
fi

#
# can the user add users?
#

yn=f

while (test "$yn" != y -a "$yn" != n)
do
    echo -n "Is user \"$NEWUSER\" allowed to add users? (y/n) "
    read yn
done

if (test "$yn" = y)
then
    CANADDUSER=t
else
    CANADDUSER=f
fi

QUERY="append pg_user (usename=\"$NEWUSER\", usesysid=$SYSID, \
      usecreatedb=\"$CANCREATE\", usetrace=\"t\", usesuper=\"$CANADDUSER\",
      usecatupd=\"t\")"

RES=`$MONITOR -TN -c "$QUERY" template1`

#
# Wrap things up.  If the user was created successfully, AND the user was
# NOT allowed to create databases, remind the DBA to create one for the user.
#

if (test $? -ne 0)
then
    echo "$0: $NEWUSER was NOT added successfully"
else
    echo "$0: $NEWUSER was successfully added"
    if (test $CANCREATE = f)
    then
        echo "don't forget to create a database for $NEWUSER"
    fi
fi
