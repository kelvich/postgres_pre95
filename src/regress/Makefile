#
# $Header$
#
.include <postgres.global.mk>

SUBDIR= bench demo regress video

#
# override default target
#.MAIN: runtest

runtest:
	@for entry in demo regress video; do \
		echo running $$entry...; \
		cd ${.CURDIR}/$$entry; \
		bmake runtest; \
	done
	@echo To run the Wisconsin benchmark, you must kill the postmaster,
	@echo cd into "bench" and "make runtest".

.include <postgres.subdir.mk>

#
# and pass runtest to all subdirectories
#
#runtest: _SUBDIRUSE
