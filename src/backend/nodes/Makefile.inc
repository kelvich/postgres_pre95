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
NODESFILES= ${ND}/nodes.h ${ND}/plannodes.h ${ND}/primnodes.h \
	${ND}/relation.h ${ND}/execnodes.h ${ND}/mnodes.h ${ND}/pg_lisp.h \
	${ND}/execnodes.a.h ${ND}/plannodes.a.h ${ND}/primnodes.a.h  \
	${ND}/relation.a.h
INHERITS= ${ND}/inherits.sh
INHERITSFILES= tags.h inh.c slots

${INHERITSFILES}: ${INHERITS} ${NODESFILES}
	sh ${SHOPTS} ${INHERITS} ${NODESFILES}

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

plannodes.c: plannodes.h GENCREATOR
primnodes.c: primnodes.h GENCREATOR
relation.c:  relation.h  GENCREATOR
execnodes.c: execnodes.h GENCREATOR

.depend: ${GENFILES} ${INHERITSFILES} ${GENCREATORFILES}

${PROG}: tags.h ${GENFILES}

CLEANFILES+= ${GENFILES} ${INHERITSFILES} ${GENCREATORFILES}

SRCS+= inh.c ${GENCREATORFILES}

HEADERS+= ${NODESFILES:T} ${GENFILES}
