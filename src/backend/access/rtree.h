/*
 *  rtree.h -- common declarations for the rtree access method code.
 *
 *	$Header$
 */

#ifndef RTreeHIncluded
#define RTreeHIncluded

/* see rtstrat.c for what all this is about */
#define RTNStrategies			8
#define RTLeftStrategyNumber		1
#define RTOverLeftStrategyNumber	2
#define RTOverlapStrategyNumber		3
#define RTOverRightStrategyNumber	4
#define RTRightStrategyNumber		5
#define RTSameStrategyNumber		6
#define RTContainsStrategyNumber	7
#define RTContainedByStrategyNumber	8

#define RTNProcs			3
#define RT_UNION_PROC			1
#define RT_INTER_PROC			2
#define RT_SIZE_PROC			3

#define F_LEAF		(1 << 0)

typedef struct RTreePageOpaqueData {
	uint32		flags;
} RTreePageOpaqueData;

typedef RTreePageOpaqueData	*RTreePageOpaque;

/*
 *  When we descend a tree, we keep a stack of parent pointers.
 */

typedef struct RTSTACK {
	struct RTSTACK *rts_parent;
	OffsetNumber   rts_child;
	BlockNumber    rts_blk;
} RTSTACK;

/*
 *  When we're doing a scan, we need to keep track of the parent stack
 *  for the marked and current items.  Also, rtrees have the following
 *  property:  if you're looking for the box (1,1,2,2), on the internal
 *  nodes you have to search for all boxes that *contain* (1,1,2,2), and
 *  not the ones that match it.  We have a private scan key for internal
 *  nodes in the opaque structure for rtrees for this reason.  See
 *  access/index-rtree/rtscan.c and rtstrat.c for how it gets initialized.
 */

typedef struct RTreeScanOpaqueData {
	struct RTSTACK	*s_stack;
	struct RTSTACK	*s_markstk;
	uint16		s_internalNKey;
	ScanKeyData	s_internalKey;
} RTreeScanOpaqueData;

typedef RTreeScanOpaqueData	*RTreeScanOpaque;

/* root page of an rtree */
#define P_ROOT		0

#endif /* RTreeHIncluded */
