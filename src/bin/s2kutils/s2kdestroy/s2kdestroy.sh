#!/bin/sh
# $Header$
#
MYORG=`s2korg`
MYUID=`pg_id`
KRBTKFILE=/tmp/tkt$MYUID@$MYORG; export KRBTKFILE
echo "Ticket file:	$KRBTKFILE"
kdestroy $*
