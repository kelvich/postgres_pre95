#!/bin/sh
#
# $Header$
#

IPCS=/bin/ipcs
AWK=/usr/bin/awk
    $IPCS | egrep '^m .*|^s .*' | egrep "`whoami`|postgres|picasso" | \
    $AWK '{printf "ipcrm -%s %s\n", $1, $2}' '-' > /tmp/ipcclean.$$

chmod +x /tmp/ipcclean.$$
if (test -s /tmp/ipcclean.$$)
then
	/tmp/ipcclean.$$
fi
rm -f /tmp/ipcclean.$$
