#
# $Header$
#
.include <postgres.global.mk>

SRCS=	manual.me \
	manual-arch.fig \
	manual-er.fig \
	manual-files.fig

EPS=	manual-arch.eps \
	manual-er.eps \
	manual-files.eps

MANUAL=manual.ps

all: ${MANUAL}

install: ${MANUAL}
	${INSTALL} -m 444 ${MANUAL} ${POSTDOCDIR}/${MANUAL}

${MANUAL}: ${SRCS} ${EPS}

.include <postgres.prog.mk>
