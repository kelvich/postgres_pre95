
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
 * catname.h --
 *	POSTGRES system catalog relation name definitions.
 *
 * Identification:
 *	$Header$
 */

#ifndef	CatNameIncluded	/* Include this file only once. */
#define CatNameIncluded	1

#include "c.h"

#include "name.h"

extern Name	AggregateRelationName;
extern Name	AccessMethodRelationName;
extern Name	AccessMethodOperatorRelationName;
extern Name	AttributeRelationName;
extern Name	DatabaseRelationName;
extern Name	DefaultsRelationName;
extern Name	DemonRelationName;
extern Name	IndexRelationName;
extern Name	InheritProcedureRelationName;
extern Name	InheritsRelationName;
extern Name	InheritancePrecidenceListRelationName;
extern Name	LanguageRelationName;
extern Name	LogRelationName;
extern Name	MagicRelationName;
extern Name	OperatorClassRelationName;
extern Name	OperatorRelationName;
extern Name	ProcedureArgumentRelationName;
extern Name	ProcedureRelationName;
extern Name	RelationRelationName;
extern Name	RulePlansRelationName;
extern Name	RuleRelationName;
extern Name	ServerRelationName;
extern Name	StatisticRelationName;
extern Name	TimeRelationName;
extern Name	TypeRelationName;
extern Name	UserRelationName;
extern Name	VariableRelationName;
extern Name	VersionRelationName;

/*
 * NameIsSystemRelationName --
 *	True iff name is the name of a system catalog relation.
 */
extern
bool
NameIsSystemRelationName ARGS((
	Name	name
));

/*
 * NameIsSharedSystemRelationName --
 *	True iff name is the name of a shared system catalog relation.
 */
extern
bool
NameIsSharedSystemRelationName ARGS((
	Name	name
));

#endif	/* !defined(CatNameIncluded) */
