#
# $Header$
#

.include <postgres.global.mk>

#
# try locating libpq.a in the following places (bletch...)
#
L1= ${.CURDIR}/../../libpq/${MAKEOBJDIR}/libpq.a
L2= ${.CURDIR}/../../libpq/libpq.a
L3= ${LIBDIR}/libpq.a
L4= ${.CURDIR}/../../../lib/libpq.a

.if exists(${L1})
LIBPQ= ${L1}
.elif exists(${L2})
LIBPQ= ${L2}
.elif exists(${L3})
LIBPQ= ${L3}
.elif exists(${L4})
LIBPQ= ${L4}
.elifmake all || runtest
.BEGIN:
        @echo "regress: Makefile error: can't find where you installed libpq.a,"
        @echo 'try running "bmake all install" again.'
${PROG}: STOP_THE_MAKEFILE
        false
.endif

LDADD+= ${LIBPQ}

#
# build sample apps
#

TESTPROGS= iportal aportal

TESTOBJS= iportal.o aportal.o

iportal: iportal.o
	${CC} ${LDFLAGS} -o ${.TARGET} ${.ALLSRC} ${LDADD}

aportal: aportal.o
	${CC} ${LDFLAGS} -o ${.TARGET} ${.ALLSRC} ${LDADD}

.include <postgres.prog.mk>

#
# build dynamically-loaded object files
#
DLOBJS= regress.o ufp1.o ufp2.o ufp3.o

#
# ... plus test query inputs
#
CREATEFILES= ${DLOBJS} \
	${TESTPROGS} \
	${TESTOBJS} \
	create.pq queries.pq errors.pq destroy.pq \
	aportalcreate.pq aportalappend.pq \
	fstest.sh aportaltest.sh

#
# ... plus exports files
#
.if defined(EXPSUFF)
CREATEFILES+= ${DLOBJS:S/.o/${EXPSUFF}/g}
.  if (${PORTNAME} == "aix")
UFP: .USE
	${LD} -H512 -T512 -o ${.TARGET} -e _nostart \
		-bI:${LIBDIR}/pg_ufp.exp -bE:$*${EXPSUFF} $*.o \
		-lm -lc 2>/dev/null
ufp1.so: $*.o $*.exp UFP
ufp2.so: $*.o $*.exp UFP
ufp3.so: $*.o $*.exp UFP
.  endif
.endif

#
# ... plus shared libraries
#
.if defined(SLSUFF)
.  if (${SLSUFF} != ".o")
CREATEFILES+= ${DLOBJS:S/.o/${SLSUFF}/g}
.    if (${PORTNAME} == "alpha")
CLEANFILES+= so_locations
.    endif
.  endif
.endif

OUTFILES= stud_emp.data onek.data regress.out aportal.out
CLEANFILES+= ${CREATEFILES} ${OUTFILES}

${OUTFILES}: ${CREATEFILES} ${PROG}
	${SHCMD} ${.CURDIR}/regress.sh 2>&1 | tee regress.out
	@echo "RESULTS OF REGRESSION ARE SAVED IN ${MAKEOBJDIR}/regress.out"

#
# prepare to run the test (including clean-up after the last run)
#
all: ${CREATEFILES}
	rm -f ${OUTFILES}

#
# run the test
#
runtest: regress.out
