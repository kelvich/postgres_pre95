#!/bin/sh
# ----------------------------------------------------------------
#   FILE
#	createdb	create a postgres database
#
#   DESCRIPTION
#	this program runs the monitor with the "-c" option to create
#   the requested database.
#
#   IDENTIFICATION
# 	$Header$
# ----------------------------------------------------------------

#
# find postgres tree
#

if (test -z "$POSTGRESHOME")
then
	PG=/usr/postgres
else
	PG=$POSTGRESHOME
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
elif (test -n "$POSTGRESTREE")
then
	MONITOR=$POSTGRESTREE/obj*/support/monitor
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

dbname=$USER

while (test -n "$1")
do
	case $1 in 
		-h) host=$2; shift;;
		-p) port=$2; shift;;
		 *) dbname=$1;;
	esac
	shift;
done

$MONITOR -TN -h $host -p $port -c "destroydb $dbname" template1

if (test $? -ne 0)
then
	echo "$progname: database destroy failed on $dbname."
	exit 1
fi
