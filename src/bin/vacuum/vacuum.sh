#!/bin/sh
#
#	Vacuum postgres databases
#
#	We expect that this script will be run via cron nightly.  This
#	replaces vacuumd and vcontrol in earlier releases of postgres.
#
_POSTGRESBIN_/monitor -c vacuum $*
