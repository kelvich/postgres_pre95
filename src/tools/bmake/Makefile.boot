# a very simple makefile...

CFLAGS = -Dvoid="char *" \
	-D_PATH_DEFSYSMK=\"`pwd`/mk-proto/sys.mk\" \
	-D_PATH_DEFSYSPATH=\"`pwd`/mk-proto\" \
	-D_BSD

CFILES= arch.c buf.c compat.c cond.c dir.c hash.c job.c main.c make.c \
	parse.c str.c strdup.c suff.c targ.c var.c

OFILES= arch.o buf.o compat.o cond.o dir.o hash.o job.o main.o make.o \
	parse.o str.o strdup.o suff.o targ.o var.o

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
	cd lst.lib; cc -I.. -c ${LSTCFILES}
	cc ${CFLAGS} ${OFILES} ${LSTOFILES} -o bootmake
	rm -f ${OFILES} ${LSTOFILES}
	./bootmake
	./bootmake install
	rm -f bootmake
