#
# $Header$
#

#
# ################# BEGIN OS-DEPENDENT SECTION #################
#

#PORTNAME=alpha
#PORTNAME=hpux
#PORTNAME=linux
#PORTNAME=sparc
PORTNAME=ultrix4

# void/const are broken in some compilers
#	define this on Ultrix, SunOS4 and HP-UX
OLD_CC=-Dvoid="char *" -Dconst=

# has __.SYMDEF in ranlibs
#	define this on Ultrix or SunOS4
HAS_RANLIB=-DHAS_SYMDEF

# <string.h> doesn't contain <strings.h>
#	define this on SunOS4
#NEED_STRINGS=-DNEED_STRINGS

# needs BSD mode turned on in headers for wait3(3) compatibility
# if "egrep -i bsd /usr/include/sys/wait.h" prints anything, you
# probably need that.  (you want "union wait" to be defined.)
#	define this on Alpha OSF/1 or HP-UX ...
#BSD_WAIT3=-D_BSD
#	... but define this on Linux or other gnulib-based systems
#BSD_WAIT3=-D__USE_BSD

# needs random stuff, most of which is in most POSIXy libraries
#	define this on nearly everything except Linux
NEED_POSIX_SRC= setenv.c strdup.c strerror.c
NEED_POSIX_OBJ= setenv.o strdup.o strerror.o

#
CONFIGS= ${OLD_CC} ${HAS_RANLIB} ${NEED_STRINGS} ${BSD_WAIT3}

#
# ################# END OS-DEPENDENT SECTION #################
#

CFLAGS = ${CONFIGS} \
	-D_PATH_DEFSYSMK=\"`pwd`/mk-proto/sys.mk\" \
	-D_PATH_DEFSYSPATH=\"`pwd`/mk-proto\"

CFILES= arch.c buf.c compat.c cond.c dir.c hash.c job.c main.c make.c \
	parse.c str.c suff.c targ.c var.c \
	${NEED_POSIX_SRC}

OFILES= arch.o buf.o compat.o cond.o dir.o hash.o job.o main.o make.o \
	parse.o str.o suff.o targ.o var.o \
	${NEED_POSIX_OBJ}

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
	cc ${CFLAGS} -I. -c ${CFILES}
	cd lst.lib; cc ${CFLAGS} -I.. -c ${LSTCFILES}
	cc ${CFLAGS} ${OFILES} ${LSTOFILES} -o bootmake
	rm -f ${OFILES} ${LSTOFILES}
	-if test ! -d obj; \
		then mkdir obj; fi
	./bootmake PORTNAME=${PORTNAME} clean bmake install
	rm -f ./bootmake
