#ifndef libpq_fs_H
#define libpq_fs_H

#include <dirent.h>
#include "tmp/simplelists.h"

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
