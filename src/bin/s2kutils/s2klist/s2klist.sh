#!/bin/sh
# $Header$
#
if [ \! "$PGREALM" ]; then
	echo "$0: PGREALM not set"
	exit 1
fi

MYUID=`pg_id`
if [ \! "$KRBVERS" ]; then
	KRBVERS=_fUnKy_KrBvErS_sTuFf_
fi
if  [ "$KRBVERS" -eq 4 ]; then
	if [ "$KRBTKFILE" ]; then
		PG_TKT_FILE=$KRBTKFILE@$PGREALM
	else
		PG_TKT_FILE=/tmp/tkt$MYUID@$PGREALM
	fi
	KRBTKFILE=$PG_TKT_FILE; export KRBTKFILE
elif [ "$KRBVERS" -eq 5 ]; then
	if [ "$KRB5CCNAME" ]; then
		PG_TKT_FILE=$KRB5CCNAME@$PGREALM
	else
		PG_TKT_FILE="FILE:/tmp/krb5cc_$MYUID@$PGREALM"
	fi
	KRB5CCNAME=$PG_TKT_FILE; export KRB5CCNAME
else
	echo No Kerberos version selected
	exit 1
fi
echo "Ticket file:	$PG_TKT_FILE"

klist $*
