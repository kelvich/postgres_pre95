/*
 *  pagedoc.c -- postgres page doctor.
 *
 *	This program understands page formats for postgres heap and index
 *	relations, and can be used to dump the pages.  it doesn't know
 *	about user data in tuples; it only knows about tuple headers.
 *
 *	Usage:
 *		pagedoc [-h|b|r] [-d level] filename
 *
 *		-h, -b, and -r are for heap, btree, and rtree files,
 *			respectively.
 *		-d level sets the detail level:  0 is just page summaries,
 *			1 is page summaries plus line pointer summaries,
 *			and 2 is 1 plus tuples.
 *
 *	-h and -d0 are the defaults.
 */

#include <stdio.h>
#include <sys/file.h>

static char *RcsId = "$Header$";

extern char	*optarg;
extern int	optind, opterr;

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

#define	HEAP	0
#define BTREE	1
#define RTREE	2

extern void		pagedoc();
extern char		*readpage();
extern void		heappage();
extern void		btreepage();
extern void		rtreepage();
extern void		showlinp();
extern void		showheaptup();
extern void		showindextup();
extern int		PageGetNEntries();
extern unsigned long	ItemPointerGetBlockNumber();

main(argc, argv)
    int argc;
    char **argv;
{
    int reltype;
    char *relname;
    int level;
    char c;
    int errs;
    int fd;

    errs = 0;
    level = 0;
    reltype = HEAP;

    while ((c = getopt(argc, argv, "hbrd:")) != EOF) {
	switch (c) {
	    case 'h':
		reltype = HEAP;
		break;
	    case 'b':
		reltype = BTREE;
		break;
	    case 'r':
		reltype = RTREE;
		break;
	    case 'd':
		level = atoi(optarg);
		if  (level < 0)
		    errs++;
		break;
	    default:
		errs++;
		break;
	}
    }

    if (optind != argc - 1)
	errs++;

    if (errs) {
	fprintf(stderr, "usage: %s [-hbr] [-d level] file\n", argv[0]);
	fflush(stderr);
	exit (1);
    }

    relname = argv[optind];
    if ((fd = open(relname, O_RDONLY, 0600)) < 0) {
	perror(relname);
	fflush(stderr);
	exit (1);
    }

    pagedoc(fd, level, reltype);

    if (close(fd) < 0) {
	fprintf("close: ");
	perror(relname);
	fflush(stderr);
	exit (1);
    }

    exit (0);
}

void
pagedoc(fd, level, reltype)
    int fd;
    int level;
    int reltype;
{
    int i;
    char *buf;

    i = 0;
    while ((buf = readpage(fd)) != (char *) NULL) {
	switch (reltype) {
	    case HEAP:
		heappage(i, buf, level);
		break;
	    case BTREE:
		btreepage(i, buf, level);
		break;
	    case RTREE:
		rtreepage(i, buf, level);
		break;
	    default:
		/* should never happen */
		fprintf(stderr, "invalid reltype %d\n", reltype);
		fflush(stderr);
		return;
	}
	i++;
    }
}

char pagebuf[8192];

char *
readpage(fd)
    int fd;
{
    int nbytes;

    nbytes = read(fd, pagebuf, 8192);
    if (nbytes == 0)
	return ((char *) NULL);
    if (nbytes < 0) {
	perror("read");
	fflush(stderr);
	exit (1);
    }
    if (nbytes != 8192) {
	fprintf(stderr, "read: expected 8192 bytes, got %d (partial page?)\n",
			nbytes);
	fflush(stderr);
	exit (1);
    }

    return (pagebuf);
}

void
heappage(pgno, buf, level)
    int pgno;
    char *buf;
    int level;
{
    PageHeaderData *phdr;
    ItemIdData *linp;
    int nlinps;
    int i;
    HeapTupleData *htup;

    phdr = (PageHeaderData *) buf;
    nlinps = PageGetNEntries(phdr);
    printf("[%03d]\tlower: %d upper: %d special: %d opaque 0x%hx (%d items)\n",
		pgno, phdr->pd_lower, phdr->pd_upper, phdr->pd_special,
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

    for (i = 0, linp = &(phdr->pd_linp[0]); i < nlinps; linp++, i++) {
	showlinp(i, linp);
	/* level > 1 means show everything */
	if (level > 1) {
	    htup = (HeapTupleData *) &(buf[linp->lp_off]);
	    showheaptup(htup);
	}
    }
}

void
btreepage(pgno, buf, level)
    int pgno;
    char *buf;
    int level;
{
    PageHeaderData *phdr;
    ItemIdData *linp;
    int nlinps;
    int i;
    BTItemData *bti;
    BTPageOpaqueData *btpo;
    BTMetaPageData *meta;

    /* if this is the btree metadata page, handle it specially */
    if (pgno == 0) {
	meta = (BTMetaPageData *) buf;
	printf("[meta]\tmagic 0x%06lx version %ld root %ld freelist %ld\n",
		meta->btm_magic, meta->btm_version, meta->btm_root,
		meta->btm_freelist);
	if (meta->btm_magic != 0x053162)
	    printf("    **** magic number is bogus!\n");
	return;
    }

    phdr = (PageHeaderData *) buf;
    nlinps = PageGetNEntries(phdr);
    printf("[%03d]\tlower: %d upper: %d special: %d opaque 0x%hx (%d items)\n",
		pgno, phdr->pd_lower, phdr->pd_upper, phdr->pd_special,
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

    for (i = 0, linp = &(phdr->pd_linp[0]); i < nlinps; linp++, i++) {
	showlinp(i, linp);
	/* level > 1 means show everything */
	if (level > 1) {
	    bti = (BTItemData *) &(buf[linp->lp_off]);
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
    ItemIdData *linp;
    int nlinps;
    int i;
    RTreePageOpaqueData *rtpo;
    IndexTupleData *itup;

    phdr = (PageHeaderData *) buf;
    rtpo = (RTreePageOpaqueData *) &(buf[phdr->pd_special]);
    nlinps = PageGetNEntries(phdr);
    printf("[%03d]\tlower: %d upper: %d special: %d opaque 0x%hx %s (%d items)\n",
	    pgno, phdr->pd_lower, phdr->pd_upper, phdr->pd_special,
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

    for (i = 0, linp = &(phdr->pd_linp[0]); i < nlinps; linp++, i++) {
	showlinp(i, linp);
	/* level > 1 means show everything */
	if (level > 1) {
	    itup = (IndexTupleData *) &(buf[linp->lp_off]);
	    showindextup(itup);
	}
    }
}

void
showlinp(itemno, linp)
    int itemno;
    ItemIdData *linp;
{
    int off;

    printf("\t{%03d}\t off %d length %d flags [",
	   itemno + 1, linp->lp_off, linp->lp_len);
    if (linp->lp_flags & LP_USED)
	printf(" LP_USED");
    if (linp->lp_flags & LP_IVALID)
	printf(" LP_IVALID");
    if (linp->lp_flags & LP_DOCNT)
	printf(" LP_DOCNT");
    if (linp->lp_flags & LP_CTUP)
	printf(" LP_CTUP");
    if (linp->lp_flags & LP_LOCK)
	printf(" LP_LOCK");
    if (linp->lp_flags & LP_ISINDEX)
	printf(" LP_ISINDEX");
    printf(" ]\n");
    if ((off = linp->lp_off) > 8192)
	printf("\t    **** off too high!\n");
    if (off & 0x3)
	printf("\t    **** off is bogus -- unaligned tuple pointer!");
    if (linp->lp_len > 8192)
	printf("\t    **** len too high!\n");
    if (!(linp->lp_flags & LP_USED))
	printf("\t    **** item not used!\n");
}

void
showheaptup(htup)
    HeapTupleData *htup;
{
    printf("\t\tlen %d ctid <%d,0,%d> chain <%d,0,%d> oid %ld\n",
	   htup->t_len, ItemPointerGetBlockNumber(htup->t_ctid.block),
	   htup->t_ctid.offset, ItemPointerGetBlockNumber(htup->t_chain.block),
	   htup->t_chain.offset, htup->t_oid);
    printf("\t\tcmin/max %d/%d xmin/max %ld/%ld tmin/max %ld/%ld\n",
	   htup->t_cmin, htup->t_cmax, htup->t_xmin, htup->t_xmax,
	   htup->t_tmin, htup->t_tmax);
    printf("\t\tnatts %d vtype %c infomask 0x%x locktype %c\n",
	   htup->t_natts, htup->t_vtype, htup->t_infomask, htup->t_locktype);
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
