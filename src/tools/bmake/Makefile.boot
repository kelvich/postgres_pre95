# a very simple makefile...

CFLAGS = -Dvoid="char *" \
	-D_PATH_DEFSYSMK=\"`pwd`/mk-proto/sys.mk\" \
	-D_PATH_DEFSYSPATH=\"`pwd`/mk-proto\" \
	-D_BSD

XYZZY: 
	@echo 'make started.'
	cc ${CFLAGS} -I. -c *.c
	cd lst.lib; cc -I.. -c *.c
	cc ${CFLAGS} *.o lst.lib/*.o -o bootmake
	rm -f *.o lst.lib/*.o
	./bootmake
	./bootmake install
	rm -f bootmake
