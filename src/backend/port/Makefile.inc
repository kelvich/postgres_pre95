
# use GNU DLD's dynamic loading routines if desired
.if defined(USE_DLD)
.PATH: ${.CURDIR}/port/dld
.include "${.CURDIR}/dld/Makefile.inc"
.endif

# other port specific stuff
.PATH: ${.CURDIR}/port/${PORTNAME}
.include "${.CURDIR}/${PORTNAME}/Makefile.inc"

