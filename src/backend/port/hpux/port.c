#include <unistd.h>
#include <pwd.h>
#include <sys/syscall.h>

#if 0
char *
readuser()
{
	struct passwd *passinfo;
	static char username[255];
	uid_t myuid = getuid();

	if ((passinfo = getpwuid(myuid)) == (struct passwd *) NULL)
		return(NULL);
	strcpy(username,passinfo->pw_name);
	return(username);
}
#endif

double 
rint(x)
	double x;
{
	if (x<0.0) 
		return((double)((long)(x-0.5)));
	else 
		return((double)((long)(x+0.5)));
}

double
cbrt(x)
	double x;
{
	return(pow(x, 1.0/3.0));
}

long
random()
{
	return((long) rand());
}

void
srandom(seed)
	int seed;
{
	srand((unsigned) seed);
}

getrusage(who, ru)
	int who;
	struct rusage *ru;
{
	return(syscall(SYS_GETRUSAGE, who, ru));
}
