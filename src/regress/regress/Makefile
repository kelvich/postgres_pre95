.include <postgres.global.mk>

#
# try locating libpq.a in the following places (bletch...)
#
L1= ${.CURDIR}/../../libpq/obj/libpq.a
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
PROG= iportal

.include <postgres.prog.mk>

CREATEFILES+= \
	regress.o ufp1.o ufp2.o ufp3.o \
	create.pq queries.pq errors.pq destroy.pq \
	fstest.sh
OUTFILES= stud_emp.data onek.data regress.out
CLEANFILES+= ${CREATEFILES} ${OUTFILES}

${OUTFILES}: ${CREATEFILES} ${PROG}
	${SHCMD} ${.CURDIR}/regress.sh 2>&1 | tee regress.out
	@echo "RESULTS OF REGRESSION ARE SAVED IN obj/regress.out"

all: ${CREATEFILES} ${PROG}
	rm -f ${OUTFILES}

runtest: ${OUTFILES}
