#
# $Header$
#
.PATH: ${.CURDIR}/rewrite

SRCS+= RemoveRewrite.c RewriteDebug.c RewriteDefine.c \
	 RewriteHandler.c RewriteManip.c RewriteSupport.c locks.c

HEADERS+= RewriteDebug.h RewriteDefine.h RewriteHandler.h RewriteManip.h \
	 RewriteSupport.h locks.h
