char *astr= "a's string";

a( x )
{
    if (x) {
	printf("A> a references b; x = %d\n", x);
	b(133);
    }else {
	printf("A> x = %d\n", x);
    }
}
