/*
 * jblib.h : jblib-specific stuff
 *
 * Copyright, supplied AS-IS, and all that.
 */
/* $Header$ */


/* JB_INIT() macro for clients */
#define JB_LIB_VERSION	"0.1"
#define JB_INIT() jb_init(JB_LIB_VERSION)

/* global variables (client may change BEFORE calling JB_INIT) */
extern int jb_debug;			/* 1=print debugging messages */
extern int jb_server_port;		/* where to look for server */
extern char *jb_server_mach;		/* machine server runs on */

/* function prototypes */
int jb_init(/* char *version */);
JBPLATTER *jb_open(/* char *platter_id, char *format, int flags */);
int jb_read(/* JBPLATTER *platter, void *buf, long blockno, long nblocks */);
int jb_write(/* JBPLATTER *platter, void *buf, long blockno, long nblocks */);
int jb_close(/* void *platter */);
int jb_istat(/* char *platter_id, JBSTATUS *buf */);
int jb_fstat(/* JBPLATTER *platter, JBSTATUS *buf */);
long jb_scanb(/* JBPLATTER *platter, long start_block, long search_max */);
long jb_scanw(/* JBPLATTER *platter, long start_block, long search_max */);
int jb_eject(/* char *platter_id */);
int jb_load(/* void */);
int jb_label(/* char *platter_id, JBLABEL *buf */);
int jb_error(/* void */);
char *jb_strerror(/* int errnum */);

/* debugging function prototypes */
int jb_offset(/* JBPLATTER *platter, long offset */);
int jb_shelf_offset(/* int shelf, int side, long offset */);

