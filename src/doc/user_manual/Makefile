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

.MAIN: manual.ps

manual.ps: ${SRCS} ${EPS}

.include <postgres.prog.mk>
