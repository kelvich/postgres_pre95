.include <postgres.global.mk>

LIBPQ= ${.CURDIR}/../../libpq/obj/libpq.a

LDADD+= ${LIBPQ}
PROG= iportal

.include <postgres.prog.mk>

CREATEFILES+= \
	regress.o ufp1.o ufp2.o ufp3.o \
	create.pq queries.pq errors.pq destroy.pq
OUTFILES= stud_emp.data onek.data regress.out
CLEANFILES+= ${CREATEFILES} ${OUTFILES}

${OUTFILES}: ${CREATEFILES} ${PROG}
	${SHCMD} ${.CURDIR}/regress.sh 2>&1 | tee regress.out
	@echo "RESULTS OF REGRESSION ARE SAVED IN obj/regress.out"

all: ${CREATEFILES} ${PROG}
	rm -f ${OUTFILES}

runtest: ${OUTFILES}
