#!/bin/sh
# $Header$
#
MYORG=`s2korg`
MYUID=`pg_id`
KRBTKFILE=/tmp/tkt$MYUID@$MYORG; export KRBTKFILE
if [ "$1" ]; then
	USER=$1
fi
kinit $USER.@$MYORG
