#
# $Header$
#

# use GNU DLD's dynamic loading routines if desired
.if defined(USE_DLD)
.PATH: ${.CURDIR}/port/dld
.include "${.CURDIR}/port/dld/Makefile.inc"
.endif

# other port specific stuff
.PATH: ${.CURDIR}/port/${PORTNAME}
.include "${.CURDIR}/port/${PORTNAME}/Makefile.inc"
