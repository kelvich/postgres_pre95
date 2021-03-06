#
# $Header$
#

#
# ################# BEGIN OS-DEPENDENT SECTION #################
#

PORTNAME=ultrix4

# void/const are broken in some compilers
OLD_CC=-Dvoid="char *" -Dconst=

# old ranlib format (has __.SYMDEF)
AR_TYPE=-DUSE_RANLIB

# <string.h> doesn't contain <strings.h>
NEED_STRINGS=

# needs BSD mode turned on in headers for wait3(3) compatibility
# if "egrep -i bsd /usr/include/sys/wait.h" prints anything, you
# probably need that.  (you want "union wait" to be defined.)
BSD_WAIT3=

CONFIGS= ${OLD_CC} ${AR_TYPE} ${NEED_STRINGS} ${BSD_WAIT3}

#
# stuff missing from the library
#
MORESRCS=strdup.c
MOREOBJS=strdup.o
MORELIBS=

#
# ################# END OS-DEPENDENT SECTION #################
#

CFLAGS = ${CONFIGS} \
	-D_PATH_DEFSYSMK=\"`pwd`/mk-proto/sys.mk\" \
	-D_PATH_DEFSYSPATH=\"`pwd`/mk-proto\"

CFILES= arch.c buf.c compat.c cond.c dir.c hash.c job.c main.c make.c \
	parse.c str.c suff.c targ.c var.c \
	${MORESRCS}

OFILES= arch.o buf.o compat.o cond.o dir.o hash.o job.o main.o make.o \
	parse.o str.o suff.o targ.o var.o \
	${MOREOBJS}

LSTCFILES= lstAppend.c lstAtEnd.c lstAtFront.c lstClose.c lstConcat.c \
	lstDatum.c lstDeQueue.c lstDestroy.c lstDupl.c lstEnQueue.c \
	lstFind.c lstFindFrom.c lstFirst.c lstForEach.c lstForEachFrom.c \
	lstInit.c lstInsert.c lstIsAtEnd.c lstIsEmpty.c lstLast.c \
	lstMember.c lstNext.c lstOpen.c lstRemove.c lstReplace.c lstSucc.c

LSTOFILES= lst.lib/lstAppend.o lst.lib/lstAtEnd.o lst.lib/lstAtFront.o \
	lst.lib/lstClose.o lst.lib/lstConcat.o lst.lib/lstDatum.o \
	lst.lib/lstDeQueue.o lst.lib/lstDestroy.o lst.lib/lstDupl.o \
	lst.lib/lstEnQueue.o lst.lib/lstFind.o lst.lib/lstFindFrom.o \
	lst.lib/lstFirst.o lst.lib/lstForEach.o lst.lib/lstForEachFrom.o \
	lst.lib/lstInit.o lst.lib/lstInsert.o lst.lib/lstIsAtEnd.o \
	lst.lib/lstIsEmpty.o lst.lib/lstLast.o lst.lib/lstMember.o \
	lst.lib/lstNext.o lst.lib/lstOpen.o lst.lib/lstRemove.o \
	lst.lib/lstReplace.o lst.lib/lstSucc.o

XYZZY:
	@echo 'make started.'
	${CC} ${CFLAGS} -I. -c ${CFILES}
	cd lst.lib; ${CC} ${CFLAGS} -I.. -c ${LSTCFILES}
	${CC} ${CFLAGS} ${OFILES} ${LSTOFILES} ${MORELIBS} -o bootmake
	rm -f ${OFILES} ${LSTOFILES}
	-if test ! -d obj; \
		then mkdir obj; fi
	./bootmake PORTNAME=${PORTNAME} clean bmake install
	rm -f ./bootmake
