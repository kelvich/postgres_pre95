
; /*
; * 
; * POSTGRES Data Base Management System
; * 
; * Copyright (c) 1988 Regents of the University of California
; * 
; * Permission to use, copy, modify, and distribute this software and its
; * documentation for educational, research, and non-profit purposes and
; * without fee is hereby granted, provided that the above copyright
; * notice appear in all copies and that both that copyright notice and
; * this permission notice appear in supporting documentation, and that
; * the name of the University of California not be used in advertising
; * or publicity pertaining to distribution of the software without
; * specific, written prior permission.  Permission to incorporate this
; * software into commercial products can be obtained from the Campus
; * Software Office, 295 Evans Hall, University of California, Berkeley,
; * Ca., 94720 provided only that the the requestor give the University
; * of California a free licence to any derived software for educational
; * and research purposes.  The University of California makes no
; * representations about the suitability of this software for any
; * purpose.  It is provided "as is" without express or implied warranty.
; * 
; */



/*
 * syscache.h --
 *	System catalog cache definitions.
 *
 * Identification:
 *	$Header$
 */

#ifndef	SysCacheIncluded
#define SysCacheIncluded

/*#define CACHEDEBUG 	/* turns DEBUG elogs on */

#include "htup.h"
#include "name.h"

/*
 *	Declarations for util/syscache.c.
 *
 *	SysCache identifiers.
 *	These must agree with (and are used to generate) lisplib/cacheids.l.
 */

#define	AMOPOPID	0
#define	AMOPSTRATEGY	1
#define	ATTNAME		2
#define	ATTNUM		3
#define	INDEXRELID	4
#define	LANNAME		5
#define	OPRNAME		6
#define	OPROID		7
#define	PRONAME		8
#define	PROOID		9
#define	RELNAME		10
#define	RELOID		11
#define	TYPNAME		12
#define	TYPOID 		13
#define RULEPLANCODE	14
#define AMNAME		15
#define CLANAME		16

/*
 *	struct cachedesc:	information needed for a call to InitSysCache()
 */
struct cachedesc {
	Name	*name;	/* this is Name * so that we can initialize it...grr */
	int	nkeys;
	int	key[4];
	int	size;	/* sizeof(appropriate struct) */
};

extern long		SearchSysCacheStruct();
extern HeapTuple	SearchSysCacheTuple();
/*
 * XXX Declarations for LISP stuff deliberately left out.
 *     You shouldn't be using them.
 */

#endif /* !SysCacheIncluded */
