/*
 *  rtree.h -- common declarations for the rtree access method code.
 *
 *	$Header$
 */

#ifndef RTreeHIncluded
#define RTreeHIncluded

/* see rtstrat.c for what all this is about */
#define RTNStrategies			7
#define RTContainedByStrategyNumber	1
#define RTIntersectsStrategyNumber	2
#define RTDisjointStrategyNumber	3
#define RTContainsStrategyNumber	4
#define RTNotCBStrategyNumber		5
#define RTNotCStrategyNumber		6
#define RTEqualStrategyNumber		7

#define RTNProcs			3
#define RT_UNION_PROC			1
#define RT_SIZE_PROC			2
#define RT_INTER_PROC			3

#define F_LEAF		(1 << 0)

typedef struct RTreePageOpaqueData {
	uint32		flags;
} RTreePageOpaqueData;

typedef RTreePageOpaqueData	*RTreePageOpaque;

#define P_ROOT		0

#endif /* RTreeHIncluded */
