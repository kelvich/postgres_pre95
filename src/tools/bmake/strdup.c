#include <stddef.h>
#include <stdlib.h>
#include <string.h>

char *
strdup(str)
        const char *str;
{
        int len;
        char *copy;

        len = strlen(str) + 1;
        if (!(copy = (char *)malloc((unsigned int)len)))
                return((char *)NULL);
        bcopy(str, copy, len);
        return(copy);
}
