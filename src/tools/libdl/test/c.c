extern char *astr;

c ( x )
{
    if (x) {
	printf("C> c references a; x = %d\n", x);
	a(0);
	printf("C> from a: %s\n", astr);
    }
}
