#ifndef libpq_fs_H
#define libpq_fs_H

#include "tmp/simplelists.h"
/* open(2) compatibility */
#define O_RDONLY        0               /* +1 == FREAD */
#define O_WRONLY        1               /* +1 == FWRITE */
#define O_RDWR          2               /* +1 == FREAD|FWRITE */
#define O_APPEND        _FAPPEND
#define O_CREAT         _FCREAT
#define O_TRUNC         _FTRUNC
#define _FAPPEND        0x0008  /* append (writes guaranteed at the end) */
#define _FCREAT         0x0200  /* open with file create */
#define _FTRUNC         0x0400  /* open with truncation */

struct  dirent {
        off_t           d_off;          /* offset of next disk dir entry */
        unsigned long   d_fileno;       /* file number of entry */
        unsigned short  d_reclen;       /* length of this record */
        unsigned short  d_namlen;       /* length of string in d_name */
        char            d_name[255+1];  /* name (up to MAXNAMLEN + 1) */
};
#define d_ino d_fileno          /* this is meaningless */

typedef struct Direntry_ {
    struct dirent d;
    SLNode Node;
} Direntry;

typedef struct PDIR_ {
    SLList dirlist;
    Direntry *current_entry;
} PDIR;

int p_open ARGS((char *fname , int mode ));
int p_close ARGS((int fd ));
int p_read ARGS((int fd , char *buf , int len ));
int p_write ARGS((int fd , char *buf , int len ));
int p_lseek ARGS((int fd , int offset , int whence ));
int p_creat ARGS((char *path , int mode ));
int p_tell ARGS((int fd ));
int p_mkdir ARGS((char *path , int mode ));
int p_unlink ARGS((char *path ));
int p_rmdir ARGS((char *path ));
int p_ferror ARGS((int fd ));
int p_rename ARGS((char *path , char *pathnew ));
int p_stat ARGS((char *path , struct stat *statbuf ));
PDIR *p_opendir ARGS((char *path ));
struct dirent *p_readdir ARGS((PDIR *dirp ));
void p_rewinddir ARGS((PDIR *dirp ));
int p_closedir ARGS((PDIR *dirp ));
int p_chdir ARGS((char *path ));
char *p_getwd ARGS((char *path ));

#endif
