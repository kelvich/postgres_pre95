
/*
 * 
 * POSTGRES Data Base Management System
 * 
 * Copyright (c) 1988 Regents of the University of California
 * 
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for educational, research, and non-profit purposes and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of the University of California not be used in advertising
 * or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Permission to incorporate this
 * software into commercial products can be obtained from the Campus
 * Software Office, 295 Evans Hall, University of California, Berkeley,
 * Ca., 94720 provided only that the the requestor give the University
 * of California a free licence to any derived software for educational
 * and research purposes.  The University of California makes no
 * representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied warranty.
 * 
 */



/*
 * catname.c --
 *	POSTGRES system catalog relation name code.
 */

#include "c.h"

#include "name.h"

#include "cat.h"

RcsId("$Header$");

static NameData	AggregateRelationNameData = { "pg_aggregate" };
static NameData	AccessMethodRelationNameData = { "pg_am" };
static NameData	AccessMethodOperatorRelationNameData = { "pg_amop" };
static NameData	AttributeRelationNameData = { "pg_attribute" };
static NameData	DatabaseRelationNameData = { "pg_database" };
static NameData	DefaultsRelationNameData = { "pg_defaults" };
static NameData	DemonRelationNameData = { "pg_demon" };
static NameData	IndexRelationNameData = { "pg_index" };
static NameData	InheritProcedureRelationNameData = { "pg_inheritproc" };
static NameData	InheritsRelationNameData = { "pg_inherits" };
static NameData	InheritancePrecidenceListRelationNameData = { "pg_ipl" };
static NameData	LanguageRelationNameData = { "pg_language" };
static NameData LogRelationNameData = { "pg_log" };
static NameData	MagicRelationNameData = { "pg_magic" };
static NameData	OperatorClassRelationNameData = { "pg_opclass" };
static NameData	OperatorRelationNameData = { "pg_operator" };
static NameData	ProcedureArgumentRelationNameData = { "pg_parg" };
static NameData	ProcedureRelationNameData = { "pg_proc" };
static NameData	RelationRelationNameData = { "pg_relation" };
static NameData	RulePlansRelationNameData = { "pg_ruleplans" };
static NameData	RuleRelationNameData = { "pg_rule" };
static NameData	ServerRelationNameData = { "pg_server" };
static NameData	StatisticRelationNameData = { "pg_statistic" };
static NameData TimeRelationNameData = { "pg_time" };
static NameData	TypeRelationNameData = { "pg_type" };
static NameData	UserRelationNameData = { "pg_user" };
static NameData	VariableRelationNameData = { "pg_variable" };
static NameData	VersionRelationNameData = { "pg_version" };

Name	AggregateRelationName = &AggregateRelationNameData;
Name	AccessMethodRelationName = &AccessMethodRelationNameData;
Name	AccessMethodOperatorRelationName =
	&AccessMethodOperatorRelationNameData;
Name	AttributeRelationName = &AttributeRelationNameData;
Name	DatabaseRelationName = &DatabaseRelationNameData;
Name	DefaultsRelationName = &DefaultsRelationNameData;
Name	DemonRelationName = &DemonRelationNameData;
Name	IndexRelationName = &IndexRelationNameData;
Name	InheritProcedureRelationName = &InheritProcedureRelationNameData;
Name	InheritsRelationName = &InheritsRelationNameData;
Name	InheritancePrecidenceListRelationName =
	&InheritancePrecidenceListRelationNameData;
Name	LanguageRelationName = &LanguageRelationNameData;
Name    LogRelationName = &LogRelationNameData;
Name	MagicRelationName = &MagicRelationNameData;
Name	OperatorClassRelationName = &OperatorClassRelationNameData;
Name	OperatorRelationName = &OperatorRelationNameData;
Name	ProcedureArgumentRelationName = &ProcedureArgumentRelationNameData;
Name	ProcedureRelationName = &ProcedureRelationNameData;
Name	RelationRelationName = &RelationRelationNameData;
Name	RulePlansRelationName = &RulePlansRelationNameData;
Name	RuleRelationName = &RuleRelationNameData;
Name	ServerRelationName = &ServerRelationNameData;
Name	StatisticRelationName = &StatisticRelationNameData;
Name    TimeRelationName = &TimeRelationNameData;
Name	TypeRelationName = &TypeRelationNameData;
Name	UserRelationName = &UserRelationNameData;
Name	VariableRelationName = &VariableRelationNameData;
Name	VersionRelationName = &VersionRelationNameData;
