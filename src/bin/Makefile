SUBDIR=  monitor pagedoc pg_id pg_version shmemdoc
SUBDIR+= createdb createuser destroydb destroyuser initdb vacuum

#
# Removed: tree xexemon.
# Subsumed pg_uid functionality into pg_id.  Just call pg_id with
# a login name argument.
#
# Also, the postmaster had so many files in common with the backend (like 80%)
# so i just made it a second target in the backend Makefile.
#

.include <postgres.subdir.mk>
