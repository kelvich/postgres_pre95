#!/bin/sh
# $Header$
#
if [ -d ./obj ]; then
	cd ./obj
fi

echo =============== destroying old regression database... =================
destroydb regression

echo =============== creating new regression database... =================
createdb regression
if [ $? -ne 0 ]; then
     echo createdb failed
     exit 1
fi

monitor regression < create.pq
if [ $? -ne 0 ]; then
     echo the creation script has an error
     exit 1
fi

echo =============== running regression queries ... =================
monitor regression < queries.pq
if [ $? -ne 0 ]; then
     echo the queries script causes an error
     exit 1
fi

echo =============== running error queries ... =================
monitor regression < errors.pq
if [ $? -ne 0 ]; then
     echo the errors script has an unanticipated problem
     exit 1
fi

monitor regression < destroy.pq
if [ $? -ne 0 ]; then
     echo the destroy script has an error
     exit 1
fi

echo =============== destroying regression database... =================
destroydb regression
if [ $? -ne 0 ]; then
     echo destroydb failed
     exit 1
fi

exit 0
