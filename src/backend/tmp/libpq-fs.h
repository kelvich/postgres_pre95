#ifndef libpq_fs_H
#define libpq_fs_H

#include "tmp/simplelists.h"
#include <sys/file.h>

/* UNIX compatibility junk.  This should be in all system''s include files,
   but this is not always the case. */

#define MAXNAMLEN 255
struct pgdirent {
	unsigned long d_ino;
	unsigned short d_namlen;
	char d_name[MAXNAMLEN+1];
};

/* for lseek(2) */
#ifndef SEEK_SET
#define SEEK_SET 0
#endif

/* for stat(2) */
#ifndef S_IRUSR /* this not defined means rest are probably not defined */
/* file modes */

#define S_IRWXU 00700           /* read, write, execute: owner */
#define S_IRUSR 00400           /*  read permission: owner */
#define S_IWUSR 00200           /*  write permission: owner */
#define S_IXUSR 00100           /*  execute permission: owner */

#define S_IRWXG 00070           /* read, write, execute: group */
#define S_IRGRP 00040           /*  read permission: group */
#define S_IWGRP 00020           /*  write permission: group */
#define S_IXGRP 00010           /*  execute permission: group */

#define S_IRWXO 00007           /* read, write, execute: other */
#define S_IROTH 00004           /*  read permission: other */
#define S_IWOTH 00002           /*  write permission: other */
#define S_IXOTH 00001           /*  execute permission: other */

#define _S_IFMT  0170000        /* type of file; sync with S_IFMT */
#define _S_IFBLK 0060000        /* block special; sync with S_IFBLK */
#define _S_IFCHR 0020000        /* character special sync with S_IFCHR */
#define _S_IFDIR 0040000        /* directory; sync with S_IFDIR */
#define _S_IFIFO 0010000        /* FIFO - named pipe; sync with S_IFIFO */
#define _S_IFREG 0100000        /* regular; sync with S_IFREG */

#undef S_IFDIR
#undef S_IFREG
#define S_IFDIR _S_IFDIR
#define S_IFREG _S_IFREG

#define S_ISDIR( mode )         (((mode) & _S_IFMT) == _S_IFDIR)

#endif

/*
 *  Flags for inversion file system large objects.  Normally, creat()
 *  takes mode arguments, but we don't use them in inversion, since
 *  you get postgres protections.  Instead, we use the low sixteen bits
 *  of the integer mode argument to store the number of the storage
 *  manager to be used, and the high sixteen bits for flags.
 */

#define INV_SMGRMASK	0x0000ffff
#define	INV_ARCHIVE	0x00010000
#define	INV_WRITE	0x00020000
#define	INV_READ	0x00040000

struct pgstat { /* just the fields we need from stat structure */
    int st_ino;
    int st_mode;
    unsigned int st_size;
    unsigned int st_sizehigh;	/* high order bits */
/* 2^64 == 1.8 x 10^20 bytes */
    int st_uid;
    int st_atime;
    int st_mtime;
    int st_ctime;
};

typedef struct Direntry_ {
    struct pgdirent d;
    SLNode Node;
} Direntry;

typedef struct PDIR_ {
    SLList dirlist;
    Direntry *current_entry;
} PDIR;

/* Error values for p_errno */
#define PEPERM           1               /* Not owner */
#define PENOENT          2               /* No such file or directory */
#define PEACCES          13              /* Permission denied */
#define PEEXIST          17              /* File exists */
#define PENOTDIR         20              /* Not a directory*/
#define PEISDIR          21              /* Is a directory */
#define PEINVAL          22              /* Invalid argument */
#define PENAMETOOLONG    63              /* File name too long */
#define PENOTEMPTY       66              /* Directory not empty */
#define PEPGIO           99              /* postgres backend had problems */

int p_open ARGS((char *fname , int mode ));
int p_close ARGS((int fd ));
int p_read ARGS((int fd , char *buf , int len ));
int p_write ARGS((int fd , char *buf , int len ));
int p_lseek ARGS((int fd , int offset , int whence ));
int p_creat ARGS((char *path , int mode, int objtype ));
int p_tell ARGS((int fd ));
int p_mkdir ARGS((char *path , int mode ));
int p_unlink ARGS((char *path ));
int p_rmdir ARGS((char *path ));
int p_ferror ARGS((int fd ));
int p_rename ARGS((char *path , char *pathnew ));
int p_stat ARGS((char *path , struct pgstat *statbuf ));
PDIR *p_opendir ARGS((char *path ));
struct pgdirent *p_readdir ARGS((PDIR *dirp ));
void p_rewinddir ARGS((PDIR *dirp ));
int p_closedir ARGS((PDIR *dirp ));
int p_chdir ARGS((char *path ));
char *p_getwd ARGS((char *path ));

#endif
