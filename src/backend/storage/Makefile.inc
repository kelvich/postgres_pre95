#
# $Header$
#
.include "${.CURDIR}/storage/buffer/Makefile.inc"
.include "${.CURDIR}/storage/file/Makefile.inc"
.include "${.CURDIR}/storage/ipc/Makefile.inc"
.include "${.CURDIR}/storage/large_object/Makefile.inc"
.include "${.CURDIR}/storage/lmgr/Makefile.inc"
.include "${.CURDIR}/storage/page/Makefile.inc"
.include "${.CURDIR}/storage/smgr/Makefile.inc"

.PATH: ${.CURDIR}/storage

HEADERS+= backendid.h block.h buf.h buf_internals.h \
	 bufmgr.h bufpage.h execipc.h fd.h form.h ipc.h ipci.h \
	 item.h itemid.h itempos.h itemptr.h jbcomm.h jblib.h \
	 jbstruct.h lmgr.h lock.h multilev.h off.h page.h pagenum.h \
	 part.h pldebug.h pos.h proc.h procq.h shmem.h  \
	sinval.h sinvaladt.h sj.h smgr.h spin.h taslock.h
