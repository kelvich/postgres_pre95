.include <postgres.global.mk>
#
# Use "bmake" (BSD 4.4 make) to run this Makefile
#
PROG= postgres

.include "${.CURDIR}/access/Makefile.inc"
.include "${.CURDIR}/bootstrap/Makefile.inc"
.include "${.CURDIR}/catalog/Makefile.inc"
.include "${.CURDIR}/commands/Makefile.inc"
.include "${.CURDIR}/executor/Makefile.inc"
.include "${.CURDIR}/lib/Makefile.inc"
.include "${.CURDIR}/libpq/Makefile.inc"
.include "${.CURDIR}/main/Makefile.inc"
.include "${.CURDIR}/nodes/Makefile.inc"
.include "${.CURDIR}/parser/Makefile.inc"
.include "${.CURDIR}/planner/Makefile.inc"
.include "${.CURDIR}/port/Makefile.inc"
.include "${.CURDIR}/rewrite/Makefile.inc"
.include "${.CURDIR}/rules/Makefile.inc"
.include "${.CURDIR}/storage/Makefile.inc"
.include "${.CURDIR}/tcop/Makefile.inc"
.include "${.CURDIR}/utils/Makefile.inc"

#
# If you change the folowing you need to "bmake clean; bmake"
#
CFLAGS+=  -DPOSTGRESHOME='"${POSTGRESHOME}"' \
	-DPOSTGRESDATA='"${POSTGRESDATA}"' \
	-I${.CURDIR}/. -I${.CURDIR}/obj \
	-I${.CURDIR}/port/${PORTNAME}

LDADD=	-lm -lln
# XXX - grot: rename tmp to something sensible
# - TRY REMOVING THIS AND SEE WHAT BREAKS
CFLAGS+= -I${.CURDIR}/tmp

# XXX - who makes this?
CLEANFILES+= y.output

# TODO: include tmp/*.h and port/?/*.h files in $HEADERS

#
# Bit of a hack.  Should redo .mk files to handle multiple
# program targets based on a common or overlapping set of
# sources.
#
.PATH: ${.CURDIR}/postmaster
postmaster: postmaster.o ${OBJS}
	${CC} -o postmaster postmaster.o ${OBJS:S/^main.o$//} ${LDADD}

# all: postmaster

# install: postmaster
	

.include <postgres.prog.mk>
