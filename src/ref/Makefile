#
# $Header$
#
# This is a really lame Makefile, but I'm tired of dealing with it..
# unfortunately we have targets generated from individual files and
# from an ORDERED SET of files, which makes it hard to do this cleanly.
# We also need paths in some places and not in others.
#
# The stupid shell scripts are just in case you DON'T have a troff/tbl suite 
# but just have awf or nroff or something else that lets you print out plain 
# man pages.
#
.include <postgres.global.mk>

.MAIN: all

MACROS=-man

PRDEV=ps

NROFF=nroff
NROFFOPTS=${MACROS}

GROFF=groff -spt
#GROFFOPTS=${MACROS} -T${PRDEV}
GROFFOPTS= -T${PRDEV} tmac.an.old

SOELIM=gsoelim

# POSTMANDIR from ../Makefile.global
PG_MAN_DIR=${POSTMANDIR}
PROG_SRC_EXT=.1src
PROG_MAN_EXT=.1
PROG_MAN_DIR=${PG_MAN_DIR}/man1
CMD_SRC_EXT=.cmdsrc
CMD_MAN_EXT=.l
CMD_MAN_DIR=${PG_MAN_DIR}/manl
LIB_SRC_EXT=.3pqsrc
LIB_MAN_EXT=.3pq
LIB_MAN_DIR=${PG_MAN_DIR}/man3
FILE_SRC_EXT=.5src
FILE_MAN_EXT=.5
FILE_MAN_DIR=${PG_MAN_DIR}/man5


.SUFFIXES: \
	${PROG_SRC_EXT} ${PROG_MAN_EXT} \
	${CMD_SRC_EXT} ${CMD_MAN_EXT} \
	${LIB_SRC_EXT} ${LIB_MAN_EXT} \
	${FILE_SRC_EXT} ${FILE_MAN_EXT}

MANPAGE: .USE
	HERE=`pwd`; cd ${.CURDIR}; \
	${SOELIM} ${.ALLSRC:S,^${.CURDIR}/,,g} > $$HERE/${.TARGET}

${PROG_SRC_EXT}${PROG_MAN_EXT}: MANPAGE
${CMD_SRC_EXT}${CMD_MAN_EXT}: MANPAGE
${LIB_SRC_EXT}${LIB_MAN_EXT}: MANPAGE
${FILE_SRC_EXT}${FILE_MAN_EXT}: MANPAGE

.PATH: \
	${.CURDIR}/files \
	${.CURDIR}/postquel \
	${.CURDIR}/unix \
	${.CURDIR}

SOURCES= \
	${.CURDIR}/introduction \
	${.CURDIR}/unix/unix${PROG_SRC_EXT} \
	${.CURDIR}/unix/createdb${PROG_SRC_EXT} \
	${.CURDIR}/unix/createuser${PROG_SRC_EXT} \
	${.CURDIR}/unix/destroydb${PROG_SRC_EXT} \
	${.CURDIR}/unix/destroyuser${PROG_SRC_EXT} \
	${.CURDIR}/unix/icopy${PROG_SRC_EXT} \
	${.CURDIR}/unix/initdb${PROG_SRC_EXT} \
	${.CURDIR}/unix/ipcclean${PROG_SRC_EXT} \
	${.CURDIR}/unix/monitor${PROG_SRC_EXT} \
	${.CURDIR}/unix/newbki${PROG_SRC_EXT} \
	${.CURDIR}/unix/pagedoc${PROG_SRC_EXT} \
	${.CURDIR}/unix/pcat${PROG_SRC_EXT} \
	${.CURDIR}/unix/pcd${PROG_SRC_EXT} \
	${.CURDIR}/unix/pls${PROG_SRC_EXT} \
	${.CURDIR}/unix/pmkdir${PROG_SRC_EXT} \
	${.CURDIR}/unix/pmv${PROG_SRC_EXT} \
	${.CURDIR}/unix/postgres${PROG_SRC_EXT} \
	${.CURDIR}/unix/postmaster${PROG_SRC_EXT} \
	${.CURDIR}/unix/ppwd${PROG_SRC_EXT} \
	${.CURDIR}/unix/prm${PROG_SRC_EXT} \
	${.CURDIR}/unix/prmdir${PROG_SRC_EXT} \
	${.CURDIR}/unix/reindexdb${PROG_SRC_EXT} \
	${.CURDIR}/unix/s2kutils${PROG_SRC_EXT} \
	${.CURDIR}/unix/shmemdoc${PROG_SRC_EXT} \
	${.CURDIR}/built-in${LIB_SRC_EXT} \
	${.CURDIR}/postquel/postquel${CMD_SRC_EXT} \
	${.CURDIR}/postquel/abort${CMD_SRC_EXT} \
	${.CURDIR}/postquel/addattr${CMD_SRC_EXT} \
	${.CURDIR}/postquel/append${CMD_SRC_EXT} \
	${.CURDIR}/postquel/attachas${CMD_SRC_EXT} \
	${.CURDIR}/postquel/begin${CMD_SRC_EXT} \
	${.CURDIR}/postquel/change_acl${CMD_SRC_EXT} \
	${.CURDIR}/postquel/close${CMD_SRC_EXT} \
	${.CURDIR}/postquel/cluster${CMD_SRC_EXT} \
	${.CURDIR}/postquel/copy${CMD_SRC_EXT} \
	${.CURDIR}/postquel/create${CMD_SRC_EXT} \
	${.CURDIR}/postquel/createdb${CMD_SRC_EXT} \
	${.CURDIR}/postquel/create_version${CMD_SRC_EXT} \
	${.CURDIR}/postquel/define_aggregate${CMD_SRC_EXT} \
	${.CURDIR}/postquel/define_function${CMD_SRC_EXT} \
	${.CURDIR}/postquel/define_index${CMD_SRC_EXT} \
	${.CURDIR}/postquel/define_operator${CMD_SRC_EXT} \
	${.CURDIR}/postquel/define_rule${CMD_SRC_EXT} \
	${.CURDIR}/postquel/define_type${CMD_SRC_EXT} \
	${.CURDIR}/postquel/define_view${CMD_SRC_EXT} \
	${.CURDIR}/postquel/delete${CMD_SRC_EXT} \
	${.CURDIR}/postquel/destroy${CMD_SRC_EXT} \
	${.CURDIR}/postquel/destroydb${CMD_SRC_EXT} \
	${.CURDIR}/postquel/end${CMD_SRC_EXT} \
	${.CURDIR}/postquel/extend_index${CMD_SRC_EXT} \
	${.CURDIR}/postquel/fetch${CMD_SRC_EXT} \
	${.CURDIR}/postquel/listen${CMD_SRC_EXT} \
	${.CURDIR}/postquel/load${CMD_SRC_EXT} \
	${.CURDIR}/postquel/merge${CMD_SRC_EXT} \
	${.CURDIR}/postquel/move${CMD_SRC_EXT} \
	${.CURDIR}/postquel/notify${CMD_SRC_EXT} \
	${.CURDIR}/postquel/purge${CMD_SRC_EXT} \
	${.CURDIR}/postquel/remove_aggregate${CMD_SRC_EXT} \
	${.CURDIR}/postquel/remove_function${CMD_SRC_EXT} \
	${.CURDIR}/postquel/remove_index${CMD_SRC_EXT} \
	${.CURDIR}/postquel/remove_operator${CMD_SRC_EXT} \
	${.CURDIR}/postquel/remove_rule${CMD_SRC_EXT} \
	${.CURDIR}/postquel/remove_type${CMD_SRC_EXT} \
	${.CURDIR}/postquel/remove_view${CMD_SRC_EXT} \
	${.CURDIR}/postquel/rename${CMD_SRC_EXT} \
	${.CURDIR}/postquel/replace${CMD_SRC_EXT} \
	${.CURDIR}/postquel/retrieve${CMD_SRC_EXT} \
	${.CURDIR}/postquel/vacuum${CMD_SRC_EXT} \
	${.CURDIR}/libpq${LIB_SRC_EXT} \
	${.CURDIR}/fastpath${LIB_SRC_EXT} \
	${.CURDIR}/large_objects${LIB_SRC_EXT} \
	${.CURDIR}/catalogs${LIB_SRC_EXT} \
	${.CURDIR}/files/files${FILE_SRC_EXT} \
	${.CURDIR}/files/bki${FILE_SRC_EXT} \
	${.CURDIR}/files/page${FILE_SRC_EXT} \
	${.CURDIR}/files/template${FILE_SRC_EXT} \
	${.CURDIR}/references \
	${.CURDIR}/toc

C_SOURCES= \
	${.CURDIR}/../backend/lib/atoms.c

KW_OBJS= \
	keyword.nr

NR_OBJS= \
	binary.nr \
	left_unary.nr \
	right_unary.nr

TR_SOURCES= \
	${.CURDIR}/postquel/binary.tbl \
	${.CURDIR}/postquel/left_unary.tbl \
	${.CURDIR}/postquel/right_unary.tbl \
	${.CURDIR}/files/page.pic

OPTAB: .USE
	HERE=`pwd`; cd ${.CURDIR}; \
	sh ${SHOPTS} ./make_optab ${.ALLSRC:S,^${.CURDIR}/,,g} > $$HERE/${.TARGET}

binary.nr:	${.CURDIR}/postquel/binary.tbl OPTAB
left_unary.nr:	${.CURDIR}/postquel/left_unary.tbl OPTAB
right_unary.nr:	${.CURDIR}/postquel/right_unary.tbl OPTAB

${KW_OBJS}:	${C_SOURCES}
	HERE=`pwd`; cd ${.CURDIR}; \
	sh ${SHOPTS} ./make_kwtab ${.ALLSRC:S,^${.CURDIR}/,,g} > $$HERE/${.TARGET}

NR_MAC_SOURCES= ${.CURDIR}/tmac.an.nr

TR_MAC_SOURCES= ${.CURDIR}/tmac.an.tr

PROG_MAN_OBJS= \
	unix${PROG_MAN_EXT} \
	createdb${PROG_MAN_EXT} \
	createuser${PROG_MAN_EXT} \
	destroydb${PROG_MAN_EXT} \
	destroyuser${PROG_MAN_EXT} \
	icopy${PROG_MAN_EXT} \
	initdb${PROG_MAN_EXT} \
	ipcclean${PROG_MAN_EXT} \
	monitor${PROG_MAN_EXT} \
	newbki${PROG_MAN_EXT} \
	pagedoc${PROG_MAN_EXT} \
	pcat${PROG_MAN_EXT} \
	pcd${PROG_MAN_EXT} \
	pls${PROG_MAN_EXT} \
	pmkdir${PROG_MAN_EXT} \
	pmv${PROG_MAN_EXT} \
	postgres${PROG_MAN_EXT} \
	postmaster${PROG_MAN_EXT} \
	ppwd${PROG_MAN_EXT} \
	prm${PROG_MAN_EXT} \
	prmdir${PROG_MAN_EXT} \
	reindexdb${PROG_MAN_EXT} \
	s2kutils${PROG_MAN_EXT} \
	shmemdoc${PROG_MAN_EXT}

CMD_MAN_OBJS= \
	postquel${CMD_MAN_EXT} \
	abort${CMD_MAN_EXT} \
	addattr${CMD_MAN_EXT} \
	append${CMD_MAN_EXT} \
	attachas${CMD_MAN_EXT} \
	begin${CMD_MAN_EXT} \
	change_acl${CMD_MAN_EXT} \
	close${CMD_MAN_EXT} \
	cluster${CMD_MAN_EXT} \
	copy${CMD_MAN_EXT} \
	create${CMD_MAN_EXT} \
	createdb${CMD_MAN_EXT} \
	create_version${CMD_MAN_EXT} \
	define_aggregate${CMD_MAN_EXT} \
	define_function${CMD_MAN_EXT} \
	define_index${CMD_MAN_EXT} \
	define_operator${CMD_MAN_EXT} \
	define_rule${CMD_MAN_EXT} \
	define_type${CMD_MAN_EXT} \
	define_view${CMD_MAN_EXT} \
	delete${CMD_MAN_EXT} \
	destroy${CMD_MAN_EXT} \
	destroydb${CMD_MAN_EXT} \
	end${CMD_MAN_EXT} \
	extend_index${CMD_MAN_EXT} \
	fetch${CMD_MAN_EXT} \
	listen${CMD_MAN_EXT} \
	load${CMD_MAN_EXT} \
	merge${CMD_MAN_EXT} \
	move${CMD_MAN_EXT} \
	notify${CMD_MAN_EXT} \
	purge${CMD_MAN_EXT} \
	remove_aggregate${CMD_MAN_EXT} \
	remove_function${CMD_MAN_EXT} \
	remove_index${CMD_MAN_EXT} \
	remove_operator${CMD_MAN_EXT} \
	remove_rule${CMD_MAN_EXT} \
	remove_type${CMD_MAN_EXT} \
	remove_view${CMD_MAN_EXT} \
	rename${CMD_MAN_EXT} \
	replace${CMD_MAN_EXT} \
	retrieve${CMD_MAN_EXT} \
	vacuum${CMD_MAN_EXT}

LIB_MAN_OBJS= \
	built-in${LIB_MAN_EXT} \
	libpq${LIB_MAN_EXT} \
	fastpath${LIB_MAN_EXT} \
	large_objects${LIB_MAN_EXT} \
	catalogs${LIB_MAN_EXT}

FILE_MAN_OBJS= \
	files${FILE_MAN_EXT} \
	bki${FILE_MAN_EXT} \
	page${FILE_MAN_EXT} \
	template${FILE_MAN_EXT}

OBJS= \
	${PROG_MAN_OBJS} \
	${CMD_MAN_OBJS} \
	${LIB_MAN_OBJS} \
	${FILE_MAN_OBJS}

${OBJS}: ${NR_MAC_SOURCES}

ref.cat: ${SOURCES} ${NR_OBJS} ${KW_OBJS} ${NR_MAC_SOURCES}
	HERE=`pwd`; cd ${.CURDIR}; \
	${NROFF} ${NROFFOPTS} ${NR_MAC_SOURCES:S,^${.CURDIR}/,,g} ${SOURCES:S,^${.CURDIR}/,,g} > $$HERE/${.TARGET}


ref.${PRDEV}: ${SOURCES} ${NR_OBJS} ${KW_OBJS} ${TR_MAC_SOURCES}
	HERE=`pwd`; cd ${.CURDIR}; \
	${GROFF} ${GROFFOPTS} ${TR_MAC_SOURCES:S,^${.CURDIR}/,,g} ${SOURCES:S,^${.CURDIR}/,,g} > $$HERE/${.TARGET}


CLEANFILES+= ${NR_OBJS} ${KW_OBJS} ref.cat ref.${PRDEV}

all: ref.cat ref.ps ${OBJS}

install: ${OBJS} all
	@-for i in ${PROG_MAN_DIR} ${CMD_MAN_DIR} ${LIB_MAN_DIR} ${FILE_MAN_DIR}; \
	do \
		if [ ! -d $$i ]; then \
			mkdir $$i; \
		fi; \
	done
	for i in ${PROG_MAN_OBJS}; do \
		install -m 444 $$i ${PROG_MAN_DIR}/$$i; \
	done; \
	for i in ${CMD_MAN_OBJS}; do \
		install -m 444 $$i ${CMD_MAN_DIR}/$$i; \
	done; \
	for i in ${LIB_MAN_OBJS}; do \
		install -m 444 $$i ${LIB_MAN_DIR}/$$i; \
	done; \
	for i in ${FILE_MAN_OBJS}; do \
		install -m 444 $$i ${FILE_MAN_DIR}/$$i; \
	done; \
	install -m 444 ref.cat ${POSTDOCDIR}/ref.cat; \
	install -m 444 ref.ps ${POSTDOCDIR}/ref.ps;

.include <postgres.prog.mk>

