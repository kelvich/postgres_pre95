/*
 * rm - for ReMoving files, directories & trees.
 */

#include <stdio.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/dir.h>
#include <sys/file.h>
#include <string.h>
#include "tmp/libpq-fs.h"

int	fflg;		/* -f force - supress error messages */
int	iflg;		/* -i interrogate user on each file */
int	rflg;		/* -r recurse */

int	errcode;	/* true if errors occured */

extern char *getenv();

void usage();

main(argc, argv)
	char *argv[];
{
	register char *arg;
	char *dbname;
	int ch,dflag;
	extern int optind;
	extern char *optarg;
	extern char *PQhost, *PQport;

	dflag = 0;
	fflg = !isatty(0);
	iflg = 0;
	rflg = 0;
	while ((ch = getopt(argc, argv, "H:P:D:fiRr")) != EOF)
	  switch(ch) {
	    case 'H':
	      PQhost = optarg;
	      break;
	    case 'P':
	      PQport = optarg;
	      break;
	    case 'D':
	      PQsetdb(optarg);
	      dflag = 1;
	      break;
	    case 'f':
	      fflg++;
	      break;
	    case 'i':
	      iflg++;
	      break;
	    case 'R':
	    case 'r':
	      rflg++;
	      break;
	    case '?':
	    default:
	      usage();
	  }

	if (!dflag) {
	    if ((dbname = getenv("DATABASE")) == (char *) NULL) {
		fprintf(stderr, "no database specified with -D option or in env var DATABASE\n");
		fflush(stderr);
		exit (1);
	    }
	    PQsetdb(dbname);
	}
	{
	    extern int p_attr_caching;
	    p_attr_caching = 0;
	}
	if ((argc-optind) > 0 && argv[optind][0] == '-') {
	    optind++;
	}
	
	if ((argc-optind) < 1 && !fflg) {
		PQfinish();
		usage();
	}



	while ((optind < argc) > 0) {
	    (void) rm(argv[optind], 0);
	    optind++;
	}
	PQfinish();
	exit(errcode != 0);
	/* NOTREACHED */
}

char	*path;		/* pointer to malloc'ed buffer for path */
char	*pathp;		/* current pointer to end of path */
int	pathsz;		/* size of path */

/*
 * Return TRUE if sucessful. Recursive with -r (rflg)
 */
rm(arg, level)
	char arg[];
{
	int ok;				/* true if recursive rm succeeded */
	struct pgstat buf;		/* for finding out what a file is */
	struct pgdirent *dp;		/* for reading a directory */
	PDIR *dirp;			/* for reading a directory */
	char prevname[MAXNAMLEN + 1];	/* previous name for -r */
	char *cp;

	if (dotname(arg)) {
		fprintf(stderr, "rm: cannot remove `.' or `..'\n");
		return (0);
	}
	if (p_stat(arg, &buf)) {
		if (!fflg) {
			fprintf(stderr, "rm: ");
			perror(arg);
			errcode++;
		}
		return (0);		/* error */
	}
	if ((buf.st_mode&S_IFMT) == S_IFDIR) {
		if (!rflg) {
			if (!fflg) {
				fprintf(stderr, "rm: %s is a directory\n", arg);
				errcode++;
			}
			return (0);
		}
		if (iflg && level != 0) {
			printf("rm: remove directory %s? ", arg);
			if (!yes())
				return (0);	/* didn't remove everything */
		}
		if (/*access(arg, R_OK|W_OK|X_OK)*/ 0 != 0) {
			if (p_rmdir(arg) == 0)
				return (1);	/* salvaged: removed empty dir */
			if (!fflg) {
				fprintf(stderr, "rm: %s not changed\n", arg);
				errcode++;
			}
			return (0);		/* error */
		}
		if ((dirp = p_opendir(arg)) == NULL) {
			if (!fflg) {
				fprintf(stderr, "rm: cannot read ");
				perror(arg);
				errcode++;
			}
			return (0);
		}
		if (level == 0)
			append(arg);
		prevname[0] = '\0';
		while ((dp = p_readdir(dirp)) != NULL) {
			if (dotname(dp->d_name)) {
				strcpy(prevname, dp->d_name);
				continue;
			}
			append(dp->d_name);
			p_closedir(dirp);
			ok = rm(path, level + 1);
			for (cp = pathp; *--cp != '/' && cp > path; )
				;
			pathp = cp;
			*cp++ = '\0';
			if ((dirp = p_opendir(arg)) == NULL) {
				if (!fflg) {
					fprintf(stderr, "rm: cannot read %s?\n", arg);
					errcode++;
				}
				if (level == 0 && *path == '\0')
					append(arg);
				break;
			}
			/* pick up where we left off */
			if (prevname[0] != '\0') {
				while ((dp = p_readdir(dirp)) != NULL &&
				    strcmp(prevname, dp->d_name) != 0)
					;
			}
			/* skip the one we just failed to delete */
			if (!ok) {
				dp = p_readdir(dirp);
				if (dp != NULL && strcmp(cp, dp->d_name)) {
					fprintf(stderr,
			"rm: internal synchronization error: %s, %s, %s\n",
						arg, cp, dp->d_name);
				}
				strcpy(prevname, dp->d_name);
			}
			if (level == 0 && *path == '\0')
				append(arg);
		}
		p_closedir(dirp);
		if (level == 0) {
			pathp = path;
			*pathp = '\0';
		}
		if (iflg) {
			printf("rm: remove %s? ", arg);
			if (!yes())
				return (0);
		}
		if (p_rmdir(arg) < 0) {
			if (!fflg || iflg) {
				fprintf(stderr, "rm: %s not removed\n", arg);
				errcode++;
			}
			return (0);
		}
		return (1);
	}

	if (iflg) {
		printf("rm: remove %s? ", arg);
		if (!yes())
			return (0);
	} else if (!fflg) {
		if ((buf.st_mode&S_IFMT) != S_IFLNK && /*access(arg, W_OK)*/0 < 0) {
			printf("rm: override protection %o for %s? ",
				buf.st_mode&0777, arg);
			if (!yes())
				return (0);
		}
	}
	if (p_unlink(arg) < 0) {
		if (!fflg || iflg) {
			fprintf(stderr, "rm: %s not removed: ", arg);
			perror("");
			errcode++;
		}
		return (0);
	}
	return (1);
}

/*
 * boolean: is it "." or ".." ?
 */
dotname(s)
	char *s;
{
	if (s[0] == '.')
		if (s[1] == '.')
			if (s[2] == '\0')
				return (1);
			else
				return (0);
		else if (s[1] == '\0')
			return (1);
	return (0);
}

/*
 * Get a yes/no answer from the user.
 */
yes()
{
	int i, b;

	i = b = getchar();
	while (b != '\n' && b != EOF)
		b = getchar();
	return (i == 'y');
}

/*
 * Append 'name' to 'path'.
 */
append(name)
	char *name;
{
	register int n;

	n = strlen(name);
	if (path == NULL) {
		pathsz = MAXNAMLEN + MAXPATHLEN + 2;
		if ((path = malloc(pathsz)) == NULL) {
			fprintf(stderr, "rm: ran out of memory\n");
			exit(1);
		}
		pathp = path;
	} else if (pathp + n + 2 > path + pathsz) {
		fprintf(stderr, "rm: path name too long: %s\n", path);
		exit(1);
	} else if (pathp != path && pathp[-1] != '/')
		*pathp++ = '/';
	strcpy(pathp, name);
	pathp += n;
}

void
usage() {
    fprintf(stderr, "usage: rm [-rif] file ...\n");
    exit(1);
}
