/*
 * catname.c --
 *	POSTGRES system catalog relation name code.
 */

#include "tmp/postgres.h"

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
static NameData	Prs2RuleRelationNameData = { "pg_prs2rule" };
static NameData	Prs2PlansRelationNameData = { "pg_prs2plans" };
static NameData	Prs2StubRelationNameData = { "pg_prs2stub" };
static NameData	RelationRelationNameData = { "pg_relation" };
static NameData RewriteRelationNameData = { "pg_rewrite" };
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
Name	Prs2RuleRelationName = &Prs2RuleRelationNameData;
Name	Prs2PlansRelationName = &Prs2PlansRelationNameData;
Name	Prs2StubRelationName = &Prs2StubRelationNameData;
Name	RelationRelationName = &RelationRelationNameData;
Name	RewriteRelationName = &RewriteRelationNameData;
Name	ServerRelationName = &ServerRelationNameData;
Name	StatisticRelationName = &StatisticRelationNameData;
Name    TimeRelationName = &TimeRelationNameData;
Name	TypeRelationName = &TypeRelationNameData;
Name	UserRelationName = &UserRelationNameData;
Name	VariableRelationName = &VariableRelationNameData;
Name	VersionRelationName = &VersionRelationNameData;
