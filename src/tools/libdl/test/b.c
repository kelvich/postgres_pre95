b( x )
{
    if (x) {
	printf("B> b references c; x = %d\n", x);
	c(12);
    }
}
