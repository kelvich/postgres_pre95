/*
 * Copyright (c) 1989 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Michael Fischbein.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lint
static char sccsid[] = "@(#)print.c	5.37 (Berkeley) 7/20/92";
#endif /* not lint */

#include <sys/param.h>
#include <sys/stat.h>
#include <fts.h>
#include <time.h>
#include <errno.h>
#include <grp.h>
#include <pwd.h>
#include <utmp.h>
#include <unistd.h>
#include <tzfile.h>
/*#include <stdlib.h>*/
#include <stdio.h>
#include <string.h>
#include "tmp/libpq-fs.h"
#include "ls.h"
#include "extern.h"

static int	printaname __P((FTSENT *, u_long, u_long));
static void	printlink __P((FTSENT *));
static void	printtime __P((time_t));
static int	printtype __P((u_int));
char *strmode __P((int,char*));

#define	IS_NOPRINT(p)	((p)->fts_number == NO_PRINT)

void
printscol(dp)
	DISPLAY *dp;
{
	register FTSENT *p;

	for (p = dp->list; p; p = p->fts_link) {
		if (IS_NOPRINT(p))
			continue;
		(void)printaname(p, dp->s_inode, dp->s_block);
		(void)putchar('\n');
	}
}

void
printlong(dp)
	DISPLAY *dp;
{
	register FTSENT *p;
	register struct pgstat *sp;
	NAMES *np;
	char buf[20];

	if (dp->list->fts_level != FTS_ROOTLEVEL && (f_longform || f_size))
		(void)printf("total %lu\n", howmany(dp->btotal, blocksize));

	for (p = dp->list; p; p = p->fts_link) {
		if (IS_NOPRINT(p))
			continue;
		sp = p->fts_statp;
		if (f_inode)
			(void)printf("%*lu ", dp->s_inode, sp->st_ino);
		if (f_size)
/*			(void)printf("%*qd ",*/
			(void)printf("%*d ",
			    dp->s_block, howmany(ST_BLOCKS(sp), blocksize));
		(void)strmode(sp->st_mode, buf);
		np = p->fts_pointer;
		(void)printf("%s %*u %-*s  %-*s  ", buf, dp->s_nlink,
		    1/*sp->st_nlink*/, dp->s_user, np->user, dp->s_group,
		    np->group);
		if (f_flags)
			(void)printf("%-*s ", dp->s_flags, np->flags);
		if (S_ISCHR(sp->st_mode) || S_ISBLK(sp->st_mode))
			(void)printf("%3d, %3d ",
			    0/*major(sp->st_rdev)*/, 0 /*minor(sp->st_rdev)*/);
		else if (dp->bcfile)
/*			(void)printf("%*s%*qd ",*/
			(void)printf("%*s%*d ",
			    8 - dp->s_size, "", dp->s_size, sp->st_size);
		else
/*			(void)printf("%*qd ", dp->s_size, sp->st_size);*/
			(void)printf("%*d ", dp->s_size, sp->st_size);
		if (f_accesstime)
			printtime(sp->st_atime);
		else if (f_statustime)
			printtime(sp->st_ctime);
		else
			printtime(sp->st_mtime);
		(void)printf("%s", p->fts_name);
		if (f_type)
			(void)printtype(sp->st_mode);
		if (S_ISLNK(sp->st_mode))
			printlink(p);
		(void)putchar('\n');
	}
}

#define	TAB	8

void
printcol(dp)
	DISPLAY *dp;
{
	extern int termwidth;
	static FTSENT **array = NULL;
	static int lastentries = -1;
	register FTSENT *p;
	register int base, chcnt, cnt, col, colwidth, num;
	int endcol, numcols, numrows, row;

	/*
	 * Have to do random access in the linked list -- build a table
	 * of pointers.
	 */
	if (dp->entries > lastentries) {
		lastentries = dp->entries;
		if (array == (FTSENT **) NULL) {
		    array = (FTSENT **) malloc(dp->entries * sizeof(FTSENT *));
		} else {
		    array = (FTSENT **) realloc(array, dp->entries * sizeof(FTSENT *));
		}

		if (array == (FTSENT **) NULL) {
			err(0, "%s", strerror(errno));
			printscol(dp);
		}
	}
	for (p = dp->list, num = 0; p; p = p->fts_link)
		if (p->fts_number != NO_PRINT)
			array[num++] = p;

	colwidth = dp->maxlen;
	if (f_inode)
		colwidth += dp->s_inode + 1;
	if (f_size)
		colwidth += dp->s_block + 1;
	if (f_type)
		colwidth += 1;

	colwidth = (colwidth + TAB) & ~(TAB - 1);
	if (termwidth < 2 * colwidth) {
		printscol(dp);
		return;
	}

	numcols = termwidth / colwidth;
	numrows = num / numcols;
	if (num % numcols)
		++numrows;

	if (dp->list->fts_level != FTS_ROOTLEVEL && (f_longform || f_size))
		(void)printf("total %lu\n", howmany(dp->btotal, blocksize));
	for (row = 0; row < numrows; ++row) {
		endcol = colwidth;
		for (base = row, chcnt = col = 0; col < numcols; ++col) {
			chcnt += printaname(array[base], dp->s_inode,
			    dp->s_block);
			if ((base += numrows) >= num)
				break;
			while ((cnt = (chcnt + TAB & ~(TAB - 1))) <= endcol) {
				(void)putchar('\t');
				chcnt = cnt;
			}
			endcol += colwidth;
		}
		(void)putchar('\n');
	}
}

/*
 * print [inode] [size] name
 * return # of characters printed, no trailing characters.
 */
static int
printaname(p, inodefield, sizefield)
	register FTSENT *p;
	u_long sizefield, inodefield;
{
	struct pgstat *sp;
	int chcnt;

	sp = p->fts_statp;
	chcnt = 0;
	if (f_inode)
		chcnt += printf("%*lu ", inodefield, sp->st_ino);
	if (f_size)
		chcnt += printf(/*"%*qd ",*/ "%*d ",
		    sizefield, howmany(ST_BLOCKS(sp), blocksize));
	chcnt += printf("%s", p->fts_name);
	if (f_type)
		chcnt += printtype(sp->st_mode);
	return (chcnt);
}

static void
printtime(ftime)
	time_t ftime;
{
	int i;
	char *longstring;

	longstring = ctime(&ftime);
	for (i = 4; i < 11; ++i)
		(void)putchar(longstring[i]);

#define	SIXMONTHS	((365 / 2) * (24*60*60))
	if (f_sectime)
		for (i = 11; i < 24; i++)
			(void)putchar(longstring[i]);
	else if (ftime + SIXMONTHS > time(NULL))
		for (i = 11; i < 16; ++i)
			(void)putchar(longstring[i]);
	else {
		(void)putchar(' ');
		for (i = 20; i < 24; ++i)
			(void)putchar(longstring[i]);
	}
	(void)putchar(' ');
}

static int
printtype(mode)
	u_int mode;
{
	switch(mode & S_IFMT) {
	case S_IFDIR:
		(void)putchar('/');
		return (1);
	case S_IFLNK:
		(void)putchar('@');
		return (1);
	case S_IFSOCK:
		(void)putchar('=');
		return (1);
	}
	if (mode & (S_IXUSR | S_IXGRP | S_IXOTH)) {
		(void)putchar('*');
		return (1);
	}
	return (0);
}

static void
printlink(p)
	FTSENT *p;
{
	int lnklen;
	char name[MAXPATHLEN + 1], path[MAXPATHLEN + 1];


	if (p->fts_level == FTS_ROOTLEVEL)
/*		(void)snprintf(name, sizeof(name), "%s", p->fts_name);*/
		(void)sprintf(name, "%s", p->fts_name);
	else 
/*		(void)snprintf(name, sizeof(name),
			       "%s/%s", p->fts_path, p->fts_name);*/
		(void)sprintf(name, "%s/%s", p->fts_path, p->fts_name);
#if 0
	if ((lnklen = readlink(name, path, sizeof(name) - 1)) == -1) {
		(void)fprintf(stderr, "\nls: %s: %s\n", name, strerror(errno));
		return;
	}
	path[lnklen] = '\0';
	(void)printf(" -> %s", path);
#endif
}

/* XXX: ASSUME sequential allocation */
int     m1[] = { 1, S_IREAD>>0, 'r', '-' };
int     m2[] = { 1, S_IWRITE>>0, 'w', '-' };
int     m3[] = { 3, S_ISUID|(S_IEXEC>>0), 's', S_IEXEC>>0, 'x', S_ISUID, 'S',
        '-' };
int     m4[] = { 1, S_IREAD>>3, 'r', '-' };
int     m5[] = { 1, S_IWRITE>>3, 'w', '-' };
int     m6[] = { 3, S_ISGID|(S_IEXEC>>3), 's', S_IEXEC>>3, 'x', S_ISGID, 'S',
        '-' };
int     m7[] = { 1, S_IREAD>>6, 'r', '-' };
int     m8[] = { 1, S_IWRITE>>6, 'w', '-' };
int     m9[] = { 3, S_ISVTX|(S_IEXEC>>6), 't', S_IEXEC>>6, 'x', S_ISVTX, 'T',
        '-'};

int     *m[] = { m1, m2, m3, m4, m5, m6, m7, m8, m9};

char *
strmode(flags,lp)
        char *lp;
        int flags;
{
        int **mp;

	bzero(lp,20); /* XXX */
	if (S_ISDIR(flags)) {
	    *lp++ = 'd';
	} else {
	    *lp++ = '-';
	}
        for (mp = &m[0]; mp < &m[sizeof (m)/sizeof (m[0])]; ) {
                register int *pairp = *mp++;
                register int n = *pairp++;

                while (n-- > 0) {
                        if ((flags&*pairp) == *pairp) {
                                pairp++;
                                break;
                        } else
                                pairp += 2;
                }
                *lp++ = *pairp;
        }
        return (lp);
}
