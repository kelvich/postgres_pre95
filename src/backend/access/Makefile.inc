#
# $Header$
#
.include "${.CURDIR}/access/common/Makefile.inc"
.include "${.CURDIR}/access/heap/Makefile.inc"
.include "${.CURDIR}/access/index/Makefile.inc"
.include "${.CURDIR}/access/index-rtree/Makefile.inc"
.include "${.CURDIR}/access/nbtree/Makefile.inc"
.include "${.CURDIR}/access/nobtree/Makefile.inc"
.include "${.CURDIR}/access/transam/Makefile.inc"

.PATH: ${.CURDIR}/access

HEADERS+= att.h attnum.h attval.h \
	 btree-externs.h btree.h ftup.h funcindex.h \
	 genam.h heapam.h hio.h hrnd.h htup.h ibit.h imark.h iqual.h \
	 isop.h istrat.h itup.h nbtree.h newam.h \
	 nobtree.h printtup.h relscan.h rtree.h rulescan.h sdir.h \
	 skey.h tqual.h transam.h tupdesc.h tupmacs.h valid.h \
	 xact.h xcxt.h xlog.h xtim.h
