.include <postgres.global.mk>

clean depend lint tags:

install:
	install -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/mkdep.sh ${DESTDIR}/${BINDIR}/mkdep

.include <postgres.prog.mk>
