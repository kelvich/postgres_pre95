#
# $Header$
#

.PATH: ${.CURDIR}/commands

SRCS+= async.c command.c copy.c defind.c define.c manip.c purge.c \
	remove.c rename.c vacuum.c version.c view.c

HEADERS+= command.h copy.h defrem.h manip.h vacuum.h version.h
