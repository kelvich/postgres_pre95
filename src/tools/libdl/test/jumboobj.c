#include <stdio.h>
#include <math.h>
#include <string.h>
#include <ctype.h>

/*
 * dbl-- test R_REFHI followed by R_REFLO's
 */
double x= 2.0;

dbl()
{
    double t;
    
    t = x/3;
    printf("2.0/3: %lf\n", t);
}

/*
 * trigo-- test trig functions
 */
void trigo (alpha)
     float alpha;
{
    float sa, ca, ta;
    
    printf("Hello\n");
    printf("Alpha = %lf\n",alpha);
    sa = sin(alpha); ca = cos(alpha); ta = tan(alpha);
    printf("sin = %lf cos = %lf tan = %lf\n", sa,ca,ta);
    printf("Goodbye\n");
}


/*
 * teststaticbug-- test static
 */
static char *ptrtyp[]={ "", "!", "@", "~", "*", "&", "#m", "#s", "#p", "%m", "%s",
			    "%p", "%", "#", ">", "", "^", "\\",
			    "", "", "+", "", "", "", "", NULL};


int teststaticbug(i)
     int i;
{
    int j;
    
    printf("about to reference ptrtyp\n");
    
    j=0;
    while(ptrtyp[j]) {
	printf("ptrtyp[%d] = \"%s\"\n", j, ptrtyp[j]);
	j++;
    }
    
    return(i);
    
}

/*
 *  test scCommon
 */
char array[1024];

int testanotherbug(i)
     int i;
{
    int j;
    
    printf("about to copy array\n");
    
    strcpy(array, "This is a test.");
    
    printf("the string is now %s.\n", array);
    
    return(i);
    
}

/* 
 * testswitchbug-- test switch
 */
int testswitchbug(i)
     int i;
{
    int j;
    
    printf("about to reference ptrtyp\n");
    
    switch(i) {
    case 0:
	j=0;
	break;
    case 1:
	j=1;
	break;
    case 2:
	j=2;
	break;
    default:
	j=3;
	break;
    }
    
    printf("ptrtyp[%d] = \"%s\"\n", j, ptrtyp[j]);
    return(i);
    
}


