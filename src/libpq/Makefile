#
# $Header$
#

.include <postgres.global.mk>

LIB= pq

.PATH:	${.CURDIR}/../backend/libpq \
	${.CURDIR}/../backend/utils/error \
	${.CURDIR}/../backend/lib

CFLAGS+= -DFRONTEND \
	-I${.CURDIR}/../backend/obj \
	-I${.CURDIR}/../backend

.ifdef KRBVERS
CFLAGS+= ${KRBFLAGS}
.endif

SRCS=	fe-dumpdata.c fe-pqexec.c fe-pqstubs.c fe-pqufs.c \
	auth.c portal.c portalbuf.c pqcomm.c pqpacket.c \
	format.c simplelists.c \

.if (${PORTNAME} == "sparc")
NOPROFILE=1
.endif

#
# We install the necessary header files from the backend directory
# into ${HEADERDIR}.  If this is installed into /usr/include/postgres
# (not the default), then frontend programs could just reference
# the header files as
#
#	#include <postgres/...>
#
# otherwise, the compile line will have to include a directive
# such as in the following example:
#
#	cc -I/usr/local/postgres/include foo.c
#
# -----
#
# We take advantage of the fact that frontend applications
# all will include "tmp/c.h" by prepending macro definitions
# to define PORTNAME and PORTNAME_name to tmp/c.h.  Otherwise
# the applications would have to define these by hand to get
# the right behavior.
#
# By the way, this is of course a total crock.  The headers
# should be laid out such that the visible public headers
# are all in a separate directory, and just that directory
# would need to be copied out or symlinked to.
#

HEADERFILES= port/${PORTNAME}/machine.h catalog/pg_user.h catalog/pg_lobj.h \
	storage/execipc.h storage/ipc.h storage/ipci.h \
	tmp/fastpath.h tmp/libpq-be.h tmp/libpq-fe.h \
	tmp/libpq-fs.h tmp/libpq.h tmp/postgres.h tmp/pqcomm.h \
	tmp/simplelists.h utils/dynamic_loader.h utils/exc.h \
	utils/exception.h utils/log.h utils/geo-decls.h libpq/auth.h

install-headers::
.if defined(OLD_MKDIR)
	@-if [ ! -d ${HEADERDIR} ]; then mkdir ${HEADERDIR}; fi
	@-if [ ! -d ${HEADERDIR}/port ]; then mkdir ${HEADERDIR}/port; fi
	@-if [ ! -d ${HEADERDIR}/port/${PORTNAME} ]; \
		then mkdir ${HEADERDIR}/port/${PORTNAME}; fi
	@-if [ ! -d ${HEADERDIR}/catalog ]; \
		then mkdir ${HEADERDIR}/catalog; fi
	@-if [ ! -d ${HEADERDIR}/storage ]; \
		then mkdir ${HEADERDIR}/storage; fi
	@-if [ ! -d ${HEADERDIR}/tmp ]; \
		then mkdir ${HEADERDIR}/tmp; fi
	@-if [ ! -d ${HEADERDIR}/utils ]; \
		then mkdir ${HEADERDIR}/utils; fi
	@-if [ ! -d ${HEADERDIR}/libpq ]; \
		then mkdir ${HEADERDIR}/libpq; fi
.endif
	-for i in ${HEADERFILES}; do \
		${INSTALL} -c -m 444 \
			${.CURDIR}/../backend/$$i ${HEADERDIR}/$$i; \
	done
	-ln ${HEADERDIR}/tmp/libpq.h ${HEADERDIR}
#	XXX - installing fmgr.h depends on the backend being built
	${INSTALL} -c -m 444 \
		${.CURDIR}/../backend/obj/fmgr.h ${HEADERDIR}/fmgr.h
	${INSTALL} -c -m 444 \
		${.CURDIR}/../backend/obj/tags.h ${HEADERDIR}/tags.h
	-rm -f ${HEADERDIR}/tmp/c.h
	-echo "#define PORTNAME ${PORTNAME}" > ${HEADERDIR}/tmp/c.h
	-echo "#define PORTNAME_${PORTNAME}" >> ${HEADERDIR}/tmp/c.h
	-cat ${.CURDIR}/../backend/tmp/c.h >> ${HEADERDIR}/tmp/c.h
	-chmod 444 ${HEADERDIR}/tmp/c.h

.include <postgres.lib.mk>

install: install-headers
