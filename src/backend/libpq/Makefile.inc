#
# The frontend libpq interfaces to the backend through these files.
#
.PATH: ${.CURDIR}/libpq

SRCS+= be-dumpdata.c be-fsstubs.c be-pqexec.c

#
# These files are shared with the frontend library.
#
SRCS+= auth.c pqcomm.c portal.c portalbuf.c pqpacket.c
