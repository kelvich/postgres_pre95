.include <postgres.global.mk>

SUBDIR= demo bench

#
# override default target
#
.MAIN: runtest

.include <postgres.subdir.mk>

#
# and pass runtest to all subdirectories
#
runtest: _SUBDIRUSE
