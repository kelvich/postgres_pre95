#!/bin/sh
#
# $Header$
#
PATH=/bin:/sbin:$PATH
TMPFILE=/tmp/ipcclean.$$
ipcs | egrep '^m .*|^s .*' | egrep "`whoami`|postgres|picasso" | \
awk '{printf "ipcrm -%s %s\n", $1, $2}' '-' > $TMPFILE
if [ -s $TMPFILE ]; then
	sh $TMPFILE
fi
rm -f $TMPFILE
