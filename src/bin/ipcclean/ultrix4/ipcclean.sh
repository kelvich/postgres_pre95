#!/bin/sh

MACHINE=dec

if (test $MACHINE = seq)
then
	IPCS=/usr/att/bin/ipcs
    $IPCS | egrep '^m .*|^s .*' | egrep "`whoami`|postgres|picasso" | \
    /bin/awk '{printf "/usr/att/bin/ipcrm -%s %s\n", $1, $2}' '-' > /tmp/ipcclean.$$
else
	IPCS=/usr/bin/ipcs
    $IPCS | egrep '^m .*|^s .*' | egrep "`whoami`|postgres|picasso" | \
    /bin/awk '{printf "ipcrm -%s %s\n", $1, $2}' '-' > /tmp/ipcclean.$$
fi


chmod +x /tmp/ipcclean.$$
if (test -s /tmp/ipcclean.$$)
then
	/tmp/ipcclean.$$
fi
rm -f /tmp/ipcclean.$$
