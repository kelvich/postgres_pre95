/*
 *  shmemdoc.c -- Shared memory doctor for POSTGRES.
 *
 *	This program lets you view the state of shared memory and
 *	semaphores after an abnormal termination.  It assumes that
 *	state is static -- that is, no other process should be changing
 *	shared memory while this one is running.  In order to use this,
 *	you should start the postmaster with the -n option (noreinit)
 *	in order to avoid reinitializing shared structures after a backend
 *	terminates abnormally.
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>

#include "tmp/c.h"
#include "support/shmemipc.h"
#include "support/shmemipci.h"
#include "utils/memutils.h"

RcsId("$Header$");

/* default port for ipc connections (used to generate ipc key) */
#define	DEF_PORT	4321
#define	DEF_NBUFS	64
#define LBUFSIZ		512
#define MAXARGS		10

extern int	optind, opterr;
extern char	*optarg;
extern int	errno;

typedef struct _Cmd {
    char	*cmd_name;
    int		cmd_nargs;
    int		(*cmd_func)();
} Cmd;

extern int	getsize();
extern void	cmdloop();
extern void	shmemsplit();
extern int	semstat();
extern int	semset();
extern int	quit();
extern int	help();
extern int	setbase();
extern int	whatis();
extern int	shmemstat();
extern int	bufdesc();
extern int	bufdescs();
extern int	buffer();
extern int	linp();
extern int	tuple();

Cmd	CmdList[] = {
    "bufdesc",		1,	bufdesc,
    "bufdescs",		0,	bufdescs,
    "buffer",		3,	buffer,
    "help",		0,	help,
    "linp",		2,	linp,
    "quit",		0,	quit,
    "semset",		2,	semset,
    "semstat",		0,	semstat,
    "setbase",		1,	setbase,
    "shmemstat",	0,	shmemstat,
    "tuple",		3,	tuple,
    "whatis",		1,	whatis,
    (char *) NULL,	0,	NULL
};

char *SemNames[] = {
    "shared memory lock",
    "binding lock",
    "buffer manager lock",
    "lock manager lock",
    "shared inval cache lock",
#ifdef SONY_JUKEBOX
    "sony jukebox cache lock",
    "jukebox spinlock",
#endif
#ifdef MAIN_MEMORY
    "main memory cache lock",
#endif
    "proc struct lock",
    "oid generation lock",
    (char *) NULL
};

typedef struct LRelId {
    unsigned long	relId;
    unsigned long	dbId;
} LRelId;

typedef struct BufferTag {
    LRelId		relId;
    unsigned long	blockNum;
} BufferTag;

typedef struct BufferDesc {
    int			freeNext;
    int			freePrev;
    unsigned int	data;
    BufferTag		tag;
    int			id;
    unsigned short	flags;
    short		bufsmgr;
    unsigned		refcount;
    char		sb_dbname[16];
    char		sb_relname[16];
#ifdef HAS_TEST_AND_SET
    slock_t		io_in_progress_lock;
#endif /* HAS_TEST_AND_SET */
} BufferDesc;

char		*Base;
char		*SharedRegion;
int		SemaphoreId;
int		NBuffers;
char		*BindingTable;
BufferDesc	*BufferDescriptors;
char		*BufferBlocks;
#ifndef HAS_TEST_AND_SET
int		*NWaitIOBackend;
#endif /* ndef HAS_TEST_AND_SET */
char		*BufHashTable;
char		*LockTableCtl;
char		*LockTableLockHash;
char		*LockTableXIDHash;
char		*SharedRegionEnd;
char		*LastKnownSMAddr;

typedef struct ItemIdData {
    unsigned		lp_off:13,		/* offset to tuple */
			lp_flags:6,		/* itemid flags */
			lp_len:13;		/* length of tuple */
} ItemIdData;

#define LP_USED		0x01	/* this line pointer is being used */
#define LP_IVALID	0x02	/* this tuple is known to be insert valid */
#define LP_DOCNT	0x04	/* this tuple continues on another page */
#define LP_CTUP		0x08	/* this is a continuation tuple */
#define LP_LOCK		0x10	/* this is a lock */
#define LP_ISINDEX	0x20	/* this is an internal index tuple */

typedef struct ItemPointerData {
    unsigned short	block[2];
    unsigned short	offset;
} ItemPointerData;

typedef struct HeapTupleData {
    unsigned int	t_len;
    ItemPointerData	t_ctid;
    ItemPointerData	t_chain;
    union {
	ItemPointerData		l_ltid;
	char			*l_lock;	/* actually a RuleLock */
    } t_lock;
    unsigned long	t_oid;
    unsigned short	t_cmin;
    unsigned short	t_cmax;
    unsigned long	t_xmin;
    unsigned long	t_xmax;
    unsigned long	t_tmin;
    unsigned long	t_tmax;
    unsigned short	t_natts;
    char		t_vtype;
    char		t_infomask;
    char		t_locktype;
    char		t_bits[1];
} HeapTupleData;

typedef struct IndexTupleData {
    ItemPointerData		t_tid;

#define ITUP_HASNULLS	0x8000
#define ITUP_HASVARLENA	0x4000
#define ITUP_HASRULES	0x2000
#define ITUP_LENMASK	0x1fff

    unsigned short		t_info;
} IndexTupleData;

typedef struct BTItemData {
    unsigned long	bti_oid;
    IndexTupleData	bti_itup;
} BTItemData;

typedef struct PageHeaderData {
    unsigned short	pd_lower;
    unsigned short	pd_upper;
    unsigned short	pd_special;
    unsigned short	pd_opaque;
    ItemIdData		pd_linp[1];		/* line pointers start here */
} PageHeaderData;

typedef struct BTPageOpaqueData {
	unsigned short	btpo_prev;
	unsigned short	btpo_next;
	unsigned short	btpo_flags;
} BTPageOpaqueData;

typedef struct BTMetaPageData {
    unsigned long	btm_magic;
    unsigned long	btm_version;
    unsigned long	btm_root;
    unsigned long	btm_freelist;
} BTMetaPageData;

typedef struct RTreePageOpaqueData {
    unsigned long	rtpo_flags;

#define RTF_LEAF	(1 << 0)

} RTreePageOpaqueData;

#define BTP_LEAF	(1 << 0)
#define BTP_ROOT	(1 << 1)
#define BTP_FREE	(1 << 2)

main(argc, argv)
    int argc;
    char **argv;
{
    int c;
    int errs;
    int key, spinkey, memkey, shmid;
    char *region;
    int portno;
    int size;

    errs = 0;
    portno = DEF_PORT;
    NBuffers = DEF_NBUFS;
    while ((c = getopt(argc, argv, "B:p:")) != EOF) {
	switch (c) {
	    case 'B':
		NBuffers = atoi(optarg);
		break;
	    case 'p':
		portno = atoi(optarg);
		break;
	    default:
		fprintf(stderr, "unknown argument %c\n", c);
		errs++;
		break;
	}
    }

    if (errs) {
	fprintf("usage: %s [-p port]\n", *argv);
	fflush(stderr);
	exit (1);
    }

    /* this has got to be a hirohamaism */
    key = SystemPortAddressGetIPCKey(portno);
    spinkey = IPCKeyGetSpinLockSemaphoreKey(key);

    SemaphoreId = semget(spinkey, 0, 0);
    if (SemaphoreId < 0) {
	fprintf(stderr, "%s: cannot attach semaphores (port %d)\n",
		*argv, portno);
	perror("semget");
	fflush(stderr);
	exit (1);
    }

    size = getsize();
    memkey = IPCKeyGetBufferMemoryKey(key);
    shmid = shmget(memkey, size, 0);

    if (shmid < 0) {
	fprintf(stderr, "%s: cannot identify shared memory by name\n", *argv);
	perror("shmget");
	fflush(stderr);
	exit (1);
    }

    Base = SharedRegion = region = (char *) shmat(shmid, 0, 0);
    if (region == (char *) -1) {
	fprintf(stderr, "%s: cannot attach shared memory\n", *argv);
	perror("shmat");
	fflush(stderr);
	exit (1);
    }
    SharedRegionEnd = region + size;

    shmemsplit(region);

    cmdloop();

    exit (0);
}

int
getsize()
{
    /* XXX compute this, someday */
    return(833820);
}

void
cmdloop()
{
    char *l;
    char lbuf[LBUFSIZ];
    char *cmd, *arg[MAXARGS];
    int nargs;
    int cmdno;
    int status;

    printf("> ");
    fflush(stdout);

    while ((l = fgets(lbuf, LBUFSIZ, stdin)) != (char *) NULL) {
	lbuf[strlen(l) - 1] = '\0';
	while (*l != '\0' && isspace(*l))
	    l++;
	cmd = l;

	if (*cmd == '\0')
	    goto nextcmd;

	/* skip command and white space following it */
	while (*l != '\0' && !isspace(*l))
	    l++;
	if (*l != '\0') {
	    *l++ = '\0';
	    while (*l != '\0' && isspace(*l))
		l++;
	}

	/* consume one argument */
	for (nargs = 0; nargs < MAXARGS && *l != '\0'; nargs++) {
	    arg[nargs] = l;
	    while (*l != '\0' && !isspace(*l))
		l++;
	    if (*l != '\0') {
		do
		    *l++ = '\0';
		while (*l != '\0' && isspace(*l));
	    }
	}

	for (cmdno = 0; CmdList[cmdno].cmd_name != (char *) NULL; cmdno++)
	    if (strcmp(CmdList[cmdno].cmd_name, cmd) == 0)
		break;

	if (CmdList[cmdno].cmd_name == (char *) NULL) {
	    printf("%s unknown, 'help' for help\n", cmd);
	} else if (CmdList[cmdno].cmd_nargs != nargs) {
	    printf("arg count mismatch: expected %d\n",
		   CmdList[cmdno].cmd_nargs);
	} else {
	    switch (nargs) {
		case 0:
		    status = (*(CmdList[cmdno].cmd_func))();
		    break;
		case 1:
		    status = (*(CmdList[cmdno].cmd_func))(arg[0]);
		    break;
		case 2:
		    status = (*(CmdList[cmdno].cmd_func))(arg[0],
							  arg[1]);
		    break;
		case 3:
		    status = (*(CmdList[cmdno].cmd_func))(arg[0],
							  arg[1],
							  arg[2]);
		    break;
		case 4:
		    status = (*(CmdList[cmdno].cmd_func))(arg[0],
							  arg[1],
							  arg[2],
							  arg[3]);
		    break;
		case 5:
		    status = (*(CmdList[cmdno].cmd_func))(arg[0],
							  arg[1],
							  arg[2],
							  arg[3],
							  arg[4]);
		    break;
		case 6:
		    status = (*(CmdList[cmdno].cmd_func))(arg[0],
							  arg[1],
							  arg[2],
							  arg[3],
							  arg[4],
							  arg[5]);
		    break;
		case 7:
		    status = (*(CmdList[cmdno].cmd_func))(arg[0],
							  arg[1],
							  arg[2],
							  arg[3],
							  arg[4],
							  arg[5],
							  arg[6]);
		    break;
		case 8:
		    status = (*(CmdList[cmdno].cmd_func))(arg[0],
							  arg[1],
							  arg[2],
							  arg[3],
							  arg[4],
							  arg[5],
							  arg[6],
							  arg[7]);
		    break;
		case 9:
		    status = (*(CmdList[cmdno].cmd_func))(arg[0],
							  arg[1],
							  arg[2],
							  arg[3],
							  arg[4],
							  arg[5],
							  arg[6],
							  arg[7],
							  arg[8]);
		    break;
		case 10:
		    status = (*(CmdList[cmdno].cmd_func))(arg[0],
							  arg[1],
							  arg[2],
							  arg[3],
							  arg[4],
							  arg[5],
							  arg[6],
							  arg[7],
							  arg[8],
							  arg[9]);
		    break;
		default:
		    fprintf(stderr, "illegal arg count %d\n", nargs);
		    fflush(stderr);
		    exit (1);
	    }

	    if (status < 0) {
		return;
	    } else if (status > 0) {
		fprintf(stderr, "%s returns %d\n", CmdList[cmdno].cmd_name,
			status);
		fflush(stderr);
	    }
	}

nextcmd:
	printf("> ");
	fflush(stdout);
    }
}

int
semstat()
{
    int i;
    int semval, sempid, semncnt, semzcnt;
    int ignore;

    for (i = 0; SemNames[i] != (char *) NULL; i++) {
	if ((semval = semctl(SemaphoreId, i, GETVAL, &ignore)) < 0) {
	    perror(SemNames[i]);
	    return(-errno);
	}
	if ((sempid = semctl(SemaphoreId, i, GETPID, &ignore)) < 0) {
	    perror(SemNames[i]);
	    return(-errno);
	}
	if ((semncnt = semctl(SemaphoreId, i, GETNCNT, &ignore)) < 0) {
	    perror(SemNames[i]);
	    return(-errno);
	}
	if ((semzcnt = semctl(SemaphoreId, i, GETZCNT, &ignore)) < 0) {
	    perror(SemNames[i]);
	    return(-errno);
	}
	printf("%s%*sval %3d, last pid %d, ncnt %d, zcnt %d\n",
		SemNames[i],
		30 - strlen(SemNames[i]), "",
		semval, sempid, semncnt, semzcnt);
    }
}

int
quit()
{
    return (-1);
}

int
help()
{
    printf("semstat\t\t\tshow status of system semaphores\n");
    printf("semset n val\t\tset semaphore n to val\n");
    printf("\n");
    printf("bufdesc n\t\tprint buffer descriptor n\n");
    printf("bufdescs\t\tprint all buffer descriptors\n");
    printf("buffer n type level\tshow contents of buffer n, which is a page from a\n");
    printf("\t\t\ttype (h,b,r) relation, with level (0,1,2) detail\n");
    printf("linp n which\t\tprint line pointer which of buffer n\n");
    printf("tuple n type which\tprint tuple which of buffer n, which is a page from\n");
    printf("\t\t\ta type (h,b,r) relation\n");
    printf("\n");
    printf("setbase val\t\tuse val as the logical shmem base address\n");
    printf("shmemstat\t\tprint shared memory layout and stats\n");
    printf("whatis ptr\t\twhat shared memory object is ptr?\n");
    printf("\n");
    printf("help\t\t\tprint this command summary\n");
    printf("quit\t\t\texit\n");

    return (0);
}

int
semset(n, val)
    char *n;
    char *val;
{
    int semno, semval;

    semno = atoi(n);
    if (semno < 0 || semno > MAX_SPINS) {
	fprintf(stderr, "sem number out of range: shd be between 0 and %d\n",
		MAX_SPINS);
	return (1);
    }

    semval = atoi(val);

    if (semctl(SemaphoreId, semno, SETVAL, semval) < 0) {
	perror("semset");
	return (1);
    }

    return (0);
}

/* XXX yikes */
void
shmemsplit(region)
    char *region;
{
    BindingTable = region;
    region += (60 + 1024 + 2040);
    region += 16;			/* XXX this 16 is mysterious to me */
    BufferDescriptors = (BufferDesc *) region;
    region += 4420;
    BufferBlocks = region;
    region += 524288;
#ifndef HAS_TEST_AND_SET
    NWaitIOBackend = (int *) region;
    region += 4;
#endif /* ndef HAS_TEST_AND_SET */
    BufHashTable = region;
    region += (68 + 1024);
    LockTableCtl = region;
    region += 60;
    LockTableLockHash = region;
    region += (72 + 1024);
    LockTableXIDHash = region;
    region += (72 + 1024);
    LastKnownSMAddr = region;
}

int
setbase(addr)
    char *addr;
{
    Base = (char *) strtol(addr, (char *) NULL, 0);
    return (0);
}

int
whatis(addr)
    char *addr;
{
    char *locn;
    char *base;
    int off;

    locn = (char *) strtol(addr, (char *) NULL, 0);
    locn = (locn - Base) + SharedRegion;

    if (locn >= BindingTable && locn < (char *) BufferDescriptors) {
	if (locn > BindingTable)
	    printf("pointer into ");
	printf("binding table\n");
    } else if (locn >= (char *) BufferDescriptors && locn < BufferBlocks) {
	base = (char *) BufferDescriptors;
	off = (locn - base) / sizeof(BufferDesc);
	if ((locn - base) % sizeof(BufferDesc) != 0)
	    printf("pointer into ");
	printf("buffer desc %d\n", off);
    } else if (locn >= BufferBlocks
#ifndef HAS_TEST_AND_SET
		&& locn < (char *) NWaitIOBackend) {
#else
		&& locn < BufHashTable) {
#endif
	base = BufferBlocks;
	off = (locn - base) / 8192;
	if ((locn - base) % 8192 != 0)
	    printf("byte %d of ", (locn - base) % 8192);
	printf("buffer %d\n", off);
#ifndef HAS_TEST_AND_SET
    } else if (locn >= (char *) NWaitIOBackend
	       && locn < BufHashTable) {
	if (locn != (char *) NWaitIOBackend)
	    printf("pointer into ");
	printf("number of backends waiting for i/o to complete\n");
#endif
    } else if (locn >= BufHashTable && locn < LockTableCtl) {
	if (locn > BufHashTable)
	    printf("pointer into ");
	printf("buffer hash table\n");
    } else if (locn >= LockTableCtl && locn < LockTableLockHash) {
	if (locn > LockTableCtl)
	    printf("pointer into ");
	printf("lock table control\n");
    } else if (locn >= LockTableLockHash && locn < LockTableXIDHash) {
	if (locn > LockTableLockHash)
	    printf("pointer into ");
	printf("lock table (lock hash)\n");
    } else if (locn >= LockTableXIDHash && locn < LastKnownSMAddr) {
	if (locn > LockTableXIDHash)
	    printf("pointer into ");
	printf("lock table (xid hash)\n");
    } else if (locn >= LastKnownSMAddr && locn < SharedRegionEnd) {
	printf("unknown shared memory pointer\n");
    } else
	printf("not a shared memory pointer\n");

    return (0);
}

#define	XLATE(p)	((((char *) p) - SharedRegion) + Base)

int
shmemstat()
{
    printf("shared region                0x%lx - 0x%lx\n",
	   XLATE(SharedRegion), XLATE(SharedRegionEnd));
    printf("binding table                0x%lx\n", XLATE(BindingTable));
    printf("buffer descriptors           0x%lx\n", XLATE(BufferDescriptors));
    printf("buffer blocks                0x%lx\n", XLATE(BufferBlocks));
#ifndef HAS_TEST_AND_SET
    printf("# backends waiting on i/o    0x%lx\n", XLATE(NWaitIOBackend));
#endif /* ndef HAS_TEST_AND_SET */
    printf("buffer hash table            0x%lx\n", XLATE(BufHashTable));
    printf("lock table control           0x%lx\n", XLATE(LockTableCtl));
    printf("lock table (lock hash)       0x%lx\n", XLATE(LockTableLockHash));
    printf("lock table (xid hash)        0x%lx\n", XLATE(LockTableXIDHash));
    printf("last known shared mem addr   0x%lx\n", XLATE(LastKnownSMAddr));

    return (0);
}

int
showbufd(i)
    int i;
{
    BufferDesc *d;

    d = &BufferDescriptors[i];
    printf("[%02d]\tnext %d, prev %d, data 0x%lx, relid %d, dbid %d, blkno %d\n", 
	    i, d->freeNext, d->freePrev, XLATE(d->data), d->tag.relId.relId,
	    d->tag.relId.dbId, d->tag.blockNum);
    printf("\tid %d, flags 0x%x, smgr %d, refcnt %d, db '%.16s', rel '%.16s'\n",
	    d->id, d->flags, d->bufsmgr, d->refcount,
	    &(d->sb_dbname[0]), &(d->sb_relname[0]));
#ifdef HAS_TEST_AND_SET
    printf("\tiolock 0xlx\n", d->io_in_progress_lock);
#endif
}

bufdescs()
{
    BufferDesc *d;
    int i;

    for (i = 0; i < NBuffers; i++)
	showbufd(i);

    return (0);
}

int
bufdesc(bno)
    char *bno;
{
    int i;

    i = atoi(bno);

    if (i < 0 || i >= NBuffers) {
	fprintf(stderr, "buffer number %d out of range (0 to %d)\n",
		i, NBuffers);
	return (1);
    }

    showbufd(i);

    return (0);
}

extern int		buffer();
extern void		heappage();
extern void		btreepage();
extern void		rtreepage();
extern void		showlinp();
extern void		showheaptup();
extern void		showindextup();
extern int		PageGetNEntries();
extern unsigned long	ItemPointerGetBlockNumber();

int
buffer(pgno, type, level)
    char *pgno;
    char *type;
    char *level;
{
    int p, l;
    int off;

    p = atoi(pgno);
    if (p < 0 || p >= NBuffers) {
	fprintf(stderr, "buffer number %d out of range (0 - %d)\n",
		p, NBuffers);
	return (1);
    }
    off = p * 8192;

    l = atoi(level);
    if (l < 0) {
	fprintf(stderr, "level %d out of range -- must be non-negative\n", l);
	return (1);
    }

    switch (*type) {
	case 'h':
	    heappage(BufferDescriptors[p].tag.blockNum,
		     &(BufferBlocks[off]), l);
	    break;
	case 'b':
	    btreepage(BufferDescriptors[p].tag.blockNum,
		     &(BufferBlocks[off]), l);
	    break;
	case 'r':
	    rtreepage(BufferDescriptors[p].tag.blockNum,
		     &(BufferBlocks[off]), l);
	    break;
	default:
	    /* should never happen */
	    fprintf(stderr, "type '%s' should be h, r, or b\n", type);
	    return (1);
    }

    return (0);
}

void
heappage(pgno, buf, level)
    int pgno;
    char *buf;
    int level;
{
    PageHeaderData *phdr;
    ItemIdData *lp;
    int nlinps;
    int i;
    HeapTupleData *htup;

    phdr = (PageHeaderData *) buf;
    nlinps = PageGetNEntries(phdr);
    printf("lower: %d upper: %d special: %d opaque 0x%hx (%d items)\n",
		phdr->pd_lower, phdr->pd_upper, phdr->pd_special,
		phdr->pd_opaque, nlinps);
    if (phdr->pd_lower < 8)
	printf("    **** lower too low!\n");
    if (phdr->pd_lower > 8192)
	printf("    **** lower too high!\n");
    if (phdr->pd_upper < 12)
	printf("    **** upper too low!\n");
    if (phdr->pd_upper > 8192)
	printf("    **** upper too high!\n");
    if (phdr->pd_special > 8192)
	printf("    **** special too high!\n");

    /* level 0 is page headers only */
    if (level == 0)
	return;

    for (i = 0, lp = &(phdr->pd_linp[0]); i < nlinps; lp++, i++) {
	showlinp(i, lp);
	/* level > 1 means show everything */
	if (level > 1) {
	    htup = (HeapTupleData *) &(buf[lp->lp_off]);
	    showheaptup(htup);
	}
    }
}

void
btreepage(pgno, buf, level)
    char *buf;
    int level;
{
    PageHeaderData *phdr;
    ItemIdData *lp;
    int nlinps;
    int i;
    BTItemData *bti;
    BTPageOpaqueData *btpo;
    BTMetaPageData *meta;

    /* if this is the btree metadata page, handle it specially */
    if (pgno == 0) {
	meta = (BTMetaPageData *) buf;
	printf("metapage: magic 0x%06lx version %ld root %ld freelist %ld\n",
		meta->btm_magic, meta->btm_version, meta->btm_root,
		meta->btm_freelist);
	if (meta->btm_magic != 0x053162)
	    printf("    **** magic number is bogus!\n");

	return;
    }

    phdr = (PageHeaderData *) buf;
    nlinps = PageGetNEntries(phdr);
    printf("lower: %d upper: %d special: %d opaque 0x%hx (%d items)\n",
		phdr->pd_lower, phdr->pd_upper, phdr->pd_special,
		phdr->pd_opaque, nlinps);
    btpo = (BTPageOpaqueData *) &(buf[phdr->pd_special]);
    printf("\tprev %d next %d", btpo->btpo_prev, btpo->btpo_next);
    if (btpo->btpo_flags & BTP_LEAF)
	printf(" <leaf>");
    if (btpo->btpo_flags & BTP_ROOT)
	printf(" <root>");
    if (!(btpo->btpo_flags & (BTP_LEAF|BTP_ROOT)))
	printf(" <internal>");
    if (btpo->btpo_flags & BTP_FREE)
	printf(" <free>");
    if (btpo->btpo_next != 0)
	printf(" (item 001 is high key on page)");
    else
	printf(" (no high key)");
    printf("\n");
    if (phdr->pd_lower < 8)
	printf("    **** lower too low!\n");
    if (phdr->pd_lower > 8192)
	printf("    **** lower too high!\n");
    if (phdr->pd_upper < 12)
	printf("    **** upper too low!\n");
    if (phdr->pd_upper > 8192)
	printf("    **** upper too high!\n");
    if (phdr->pd_special > 8192)
	printf("    **** special too high!\n");

    /* level 0 is page headers only */
    if (level == 0)
	return;

    for (i = 0, lp = &(phdr->pd_linp[0]); i < nlinps; lp++, i++) {
	showlinp(i, lp);
	/* level > 1 means show everything */
	if (level > 1) {
	    bti = (BTItemData *) &(buf[lp->lp_off]);
	    printf("\t\toid %ld ", bti->bti_oid);
	    showindextup(&bti->bti_itup);
	}
    }
}

void
rtreepage(pgno, buf, level)
    int pgno;
    char *buf;
    int level;
{
    PageHeaderData *phdr;
    ItemIdData *lp;
    int nlinps;
    int i;
    RTreePageOpaqueData *rtpo;
    IndexTupleData *itup;

    phdr = (PageHeaderData *) buf;
    rtpo = (RTreePageOpaqueData *) &(buf[phdr->pd_special]);
    nlinps = PageGetNEntries(phdr);
    printf("lower: %d upper: %d special: %d opaque 0x%hx %s (%d items)\n",
	    phdr->pd_lower, phdr->pd_upper, phdr->pd_special,
	    phdr->pd_opaque,
	    ((rtpo->rtpo_flags & RTF_LEAF) ? "<leaf>":"internal"), nlinps);
    if (phdr->pd_lower < 8)
	printf("    **** lower too low!\n");
    if (phdr->pd_lower > 8192)
	printf("    **** lower too high!\n");
    if (phdr->pd_upper < 12)
	printf("    **** upper too low!\n");
    if (phdr->pd_upper > 8192)
	printf("    **** upper too high!\n");
    if (phdr->pd_special > 8192)
	printf("    **** special too high!\n");

    /* level 0 is page headers only */
    if (level == 0)
	return;

    for (i = 0, lp = &(phdr->pd_linp[0]); i < nlinps; lp++, i++) {
	showlinp(i, lp);
	/* level > 1 means show everything */
	if (level > 1) {
	    itup = (IndexTupleData *) &(buf[lp->lp_off]);
	    showindextup(itup);
	}
    }
}

void
showlinp(itemno, lp)
    int itemno;
    ItemIdData *lp;
{
    int off;

    printf("  {%03d} off %d length %d flags [",
	   itemno + 1, lp->lp_off, lp->lp_len);
    if (lp->lp_flags & LP_USED)
	printf(" LP_USED");
    if (lp->lp_flags & LP_IVALID)
	printf(" LP_IVALID");
    if (lp->lp_flags & LP_DOCNT)
	printf(" LP_DOCNT");
    if (lp->lp_flags & LP_CTUP)
	printf(" LP_CTUP");
    if (lp->lp_flags & LP_LOCK)
	printf(" LP_LOCK");
    if (lp->lp_flags & LP_ISINDEX)
	printf(" LP_ISINDEX");
    printf(" ]\n");
    if ((off = lp->lp_off) > 8192)
	printf("    **** off too high!\n");
    if (off & 0x3)
	printf("    **** off is bogus -- unaligned tuple pointer!");
    if (lp->lp_len > 8192)
	printf("    **** len too high!\n");
    if (!(lp->lp_flags & LP_USED))
	printf("    **** item not used!\n");
}

void
showheaptup(htup)
    HeapTupleData *htup;
{
    printf("\tlen %d ctid <%d,0,%d> chain <%d,0,%d> oid %ld cmin/max %d/%d\n",
	   htup->t_len, ItemPointerGetBlockNumber(htup->t_ctid.block),
	   htup->t_ctid.offset, ItemPointerGetBlockNumber(htup->t_chain.block),
	   htup->t_chain.offset, htup->t_oid, htup->t_cmin, htup->t_cmax);
    printf("\txmin/max %ld/%ld tmin/max %ld/%ld natts %d\n",
	   htup->t_xmin, htup->t_xmax, htup->t_tmin, htup->t_tmax,
	   htup->t_natts);
    printf("\tvtype '\\%03o' infomask 0x%x locktype '%c'\n",
	   htup->t_vtype, htup->t_infomask, htup->t_locktype);
}

void
showindextup(itup)
    IndexTupleData *itup;
{
    printf("heap tid <%d,0,%d> info [",
	   ItemPointerGetBlockNumber(itup->t_tid.block), itup->t_tid.offset);
    if (itup->t_info & ITUP_HASNULLS)
	printf(" HASNULLS");
    if (itup->t_info & ITUP_HASVARLENA)
	printf(" HASVARLENA");
    if (itup->t_info & ITUP_HASRULES)
	printf(" HASRULES");
    printf(" ] length %d\n", itup->t_info & ITUP_LENMASK);
}

int
PageGetNEntries(phdr)
    PageHeaderData *phdr;
{
    int n;

    n = (phdr->pd_lower - (2 * sizeof(unsigned long))) / sizeof(ItemIdData);

    return (n);
}

unsigned long
ItemPointerGetBlockNumber(iptr)
    ItemPointerData *iptr;
{
    unsigned long b;

    b = (unsigned long) ((iptr->block[0] << 16) + iptr->block[1]);

    return (b);
}

int
linp(pgno, which)
    char *pgno;
    char *which;
{
    int p, n;
    int off;
    PageHeaderData *phdr;

    p = atoi(pgno);
    if (p < 0 || p >= NBuffers) {
	fprintf(stderr, "buffer number %d out of range (0 - %d)\n",
		p, NBuffers);
	return (1);
    }

    n = atoi(which);
    if (n < 0) {
	fprintf(stderr, "linp %d out of range -- must be non-negative\n", n);
	return (1);
    }

    off = p * 8192;
    phdr = (PageHeaderData *) &(BufferBlocks[off]);

    showlinp(n, &(phdr->pd_linp[n]));

    return (0);
}

int
tuple(pgno, type, which)
    char *pgno;
    char *type;
    char *which;
{
    int p, n;
    int off;
    PageHeaderData *phdr;
    ItemIdData *lp;
    BTItemData *bti;
    char *buf;

    p = atoi(pgno);
    if (p < 0 || p >= NBuffers) {
	fprintf(stderr, "buffer number %d out of range (0 - %d)\n",
		p, NBuffers);
	return (1);
    }

    n = atoi(which);
    if (n < 0) {
	fprintf(stderr, "linp %d out of range -- must be non-negative\n", n);
	return (1);
    }

    off = p * 8192;
    phdr = (PageHeaderData *) &(BufferBlocks[off]);
    buf = (char *) phdr;
    lp = &(phdr->pd_linp[n]);

    switch (*type) {
	case 'r':
	    showindextup((IndexTupleData *) &(buf[lp->lp_off]));
	    break;

	case 'b':
	    bti = (BTItemData *) &(buf[lp->lp_off]);
	    showindextup(&bti->bti_itup);
	    break;

	case 'h':
	    showheaptup((HeapTupleData *) &(buf[lp->lp_off]));
	    break;

	default:
	    fprintf(stderr, "type %s unknown -- try h, r, or b\n", type);
	    fflush(stderr);
	    return (1);
    }

    return (0);
}
