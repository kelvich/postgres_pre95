.include <postgres.global.mk>

.include <postgres.prog.mk>

CREATEFILES= regress.o create.pq queries.pq errors.pq destroy.pq
OUTFILES= stud_emp.data onek.data regress.out
CLEANFILES+= ${CREATEFILES} ${OUTFILES}

${OUTFILES}: ${CREATEFILES}
	sh ${.CURDIR}/regress.sh 2>&1 | tee regress.out
	@echo "RESULTS OF REGRESSION ARE SAVED IN obj/regress.out"

all: ${CREATEFILES}
	rm -f ${OUTFILES}

runtest: ${OUTFILES}
