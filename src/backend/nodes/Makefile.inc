#
# $Header$
#
# This part of the makefile is kinda weird.
#
# I did what worked -- if you can do better, by all means...
#
ND= ${.CURDIR}/nodes

.PATH: ${ND}

#
# IMPORTANT: nodes.h MUST come first (so it's first in the tags.h file)
#
NODESSRC= ${ND}/nodes.h ${ND}/plannodes.h ${ND}/primnodes.h \
	${ND}/relation.h ${ND}/execnodes.h ${ND}/mnodes.h ${ND}/pg_lisp.h
NODESFILES= ${NODESSRC} \
	${ND}/execnodes.a.h ${ND}/plannodes.a.h ${ND}/primnodes.a.h  \
	${ND}/relation.a.h
INHERITS= ${ND}/inherits.sh
INHERITSFILES= tags.h inh.c slots

${INHERITSFILES}: ${INHERITS} ${NODESFILES}
	sh ${SHOPTS} ${INHERITS} ${NODESSRC}

#
# These files are generated from the corresponding .h files
# through the inference rule defined below.
#
.SUFFIXES: .h .gen
GENACCESSORS= ${ND}/Gen_accessors.sh
GENFILES= plannodes.gen primnodes.gen execnodes.gen relation.gen
.h.gen: ${GENACCESSORS} slots
	rm -f ${.TARGET}; \
	sh ${SHOPTS} ${GENACCESSORS} ${.IMPSRC} > ${.TARGET}; \
	chmod 444 ${.TARGET}

#
# .USE is BSD make's "macro" facility
#  (just for variety...)
#
GENCREATORFILES= plannodes.c primnodes.c relation.c execnodes.c
GENCREATOR: .USE
	sh ${SHOPTS} ${ND}/Gen_creator.sh ${.ALLSRC} > ${.TARGET}

plannodes.c: plannodes.h slots GENCREATOR
primnodes.c: primnodes.h slots GENCREATOR
relation.c:  relation.h slots GENCREATOR
execnodes.c: execnodes.h slots GENCREATOR

.depend: ${GENFILES} ${INHERITSFILES} ${GENCREATORFILES}

${PROG}: ${INHERITSFILES} ${GENFILES}

CLEANFILES+= ${GENFILES} ${INHERITSFILES} ${GENCREATORFILES}

SRCS+= inh.c ${GENCREATORFILES}

HEADERS+= ${NODESFILES:T} ${GENFILES}
