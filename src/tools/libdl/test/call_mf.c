#include <stdio.h>
#include "dl.h"

/*#define PRODUCE_ERROR*/

main(argc, argv)
     int argc; char **argv;
{
    void *handle;
    void (*func)();

    if (!dl_init(argv[0])) {
	fprintf(stderr, "dl: %s\n", dl_error());
	exit(1);
    }

#ifndef PRODUCE_ERROR
    dl_setLibraries("/usr/lib/libc_G0.a:/usr/lib/libm_G0.a");
#endif

    if ((handle= (void *)dl_open("mathfunc.o", DL_NOW)) == NULL) {
	fprintf(stderr, "dl: %s\n", dl_error());
	exit(1);
    }

    func= (void(*)()) dl_sym(handle, "mathfunc");
#if DEBUG
    printf("func 0x%x\n", func);
    print_undef();
#endif
    if(func) {
	(*func)((double)1.570796);
	(*func)((double)1.047198);
    }else {
	printf("dl: %s\n", dl_error());
    }

#if DEBUG
    dl_printAllSymbols(handle);
#endif
    dl_close(handle);
#if DEBUG
    dl_printAllSymbols(handle);
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
