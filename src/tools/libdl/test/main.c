#include <stdio.h>
#include "dl.h"

main(argc, argv)
     int argc; char **argv;
{
    void *handle, *handle2, *handle3;
    void (*func)();

    if (!dl_init("main")) {
	fprintf(stderr, "dl: %s\n", dl_error());
	exit(1);
    }

    printf("loading foo.o\n");
    if ((handle= (void *)dl_open("foo.o", DL_LAZY))==NULL) {
	fprintf(stderr,"dl: %s\n", dl_error());
    }
    func= (void(*)()) dl_sym(handle, "foo");
#if DEBUG
    printf("func 0x%x\n", func);
    print_undef();
#endif
    if(func) {
	(*func)();
    }else {
	printf("dl: %s\n", dl_error());
    }

    printf("loading bar.o\n");
    if ((handle2=(void *)dl_open("bar.o", DL_LAZY))==NULL) {
	fprintf(stderr, "dl: %s\n", dl_error());
    }
    func= (void(*)()) dl_sym(handle, "foo");
#if DEBUG
    printf("func 0x%x\n", func);
    print_undef();
#endif
    if(func) {
	(*func)();
    }else {
	printf("dl: %s\n", dl_error());
    }
    /* once more! */
    func= (void(*)()) dl_sym(handle, "foo");
#if DEBUG
    printf("func 0x%x\n", func);
    print_undef();
#endif
    if(func) {
	(*func)();
    }else {
	printf("dl: %s\n", dl_error());
    }

    printf("loading baz.o\n");
    if ((handle3= (void *)dl_open("baz.o", DL_LAZY))==NULL) {
	fprintf(stderr, "dl: %s\n", dl_error());
    }
    func= (void(*)()) dl_sym(handle, "foo");
#if DEBUG
    printf("func 0x%x\n", func);
    print_undef();
#endif
    if(func) {
	(*func)();
    }else {
	printf("dl: %s\n", dl_error());
    }

#if DEBUG
    printf("foo.o's symbols: \n");
    dl_printAllSymbols(handle);
    printf("bar.o's symbols: \n");
    dl_printAllSymbols(handle2);
    printf("baz.o's symbols: \n");
    dl_printAllSymbols(handle3);
#endif
    dl_close(handle);
#if DEBUG
    printf("foo.o's symbols: \n");
    dl_printAllSymbols(handle);
#endif
    dl_close(handle2);
#if DEBUG
    printf("bar.o's symbols: \n");
    dl_printAllSymbols(handle2);
#endif
    dl_close(handle3);
#if DEBUG
    printf("baz.o's symbols: \n");
    dl_printAllSymbols(handle3);
#endif

    return 0;
}

print_undef()
{
    int count;
    char **symbols;

    symbols=dl_undefinedSymbols(&count);
    if (symbols) {
	printf("undefined symbols: \n");
	while(*symbols) {
	    printf("  %s\n", *symbols);
	    symbols++;
	}
    }
}
