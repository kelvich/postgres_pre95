/*
 * catname.c --
 *	POSTGRES system catalog relation name code.
 */

#include "tmp/postgres.h"
#include "catalog/pg_aggregate.h"
#include "catalog/pg_am.h"
#include "catalog/pg_amop.h"
#include "catalog/pg_amproc.h"
#include "catalog/pg_attribute.h"
#include "catalog/pg_database.h"
#include "catalog/pg_defaults.h"
#include "catalog/pg_demon.h"
#include "catalog/pg_group.h"
#include "catalog/pg_index.h"
#include "catalog/pg_inheritproc.h"
#include "catalog/pg_inherits.h"
#include "catalog/pg_ipl.h"
#include "catalog/pg_language.h"
#include "catalog/pg_listener.h"
#include "catalog/pg_lobj.h"
#include "catalog/pg_log.h"
#include "catalog/pg_magic.h"
#include "catalog/pg_naming.h"
#include "catalog/pg_opclass.h"
#include "catalog/pg_operator.h"
#include "catalog/pg_parg.h"
#include "catalog/pg_platter.h"
#include "catalog/pg_plmap.h"
#include "catalog/pg_proc.h"
#include "catalog/pg_prs2plans.h"
#include "catalog/pg_prs2rule.h"
#include "catalog/pg_prs2stub.h"
#include "catalog/pg_relation.h"
#include "catalog/pg_rewrite.h"
#include "catalog/pg_server.h"
#include "catalog/pg_statistic.h"
#include "catalog/pg_time.h"
#include "catalog/pg_type.h"
#include "catalog/pg_user.h"
#include "catalog/pg_variable.h"
#include "catalog/pg_version.h"

RcsId("$Header$");

static NameData	AggregateRelationNameData = { Name_pg_aggregate };
static NameData	AccessMethodRelationNameData = { Name_pg_am };
static NameData	AccessMethodOperatorRelationNameData = { Name_pg_amop };
static NameData	AccessMethodProcedureRelationNameData = { Name_pg_proc };
static NameData	AttributeRelationNameData = { Name_pg_attribute };
static NameData	DatabaseRelationNameData = { Name_pg_database };
static NameData	DefaultsRelationNameData = { Name_pg_defaults };
static NameData	DemonRelationNameData = { Name_pg_demon };
static NameData	GroupRelationNameData = { Name_pg_group };
static NameData	IndexRelationNameData = { Name_pg_index };
static NameData	InheritProcedureRelationNameData = { Name_pg_inheritproc };
static NameData	InheritsRelationNameData = { Name_pg_inherits };
static NameData	InheritancePrecidenceListRelationNameData = { Name_pg_ipl };
static NameData	LanguageRelationNameData = { Name_pg_language };
static NameData LargeObjectAssocRelationNameData = { Name_pg_large_object };
static NameData ListenerRelationNameData = { Name_pg_listener };
static NameData LogRelationNameData = { Name_pg_log };
static NameData	MagicRelationNameData = { Name_pg_magic };
static NameData NamingRelationNameData = { Name_pg_naming };
static NameData	OperatorClassRelationNameData = { Name_pg_opclass };
static NameData	OperatorRelationNameData = { Name_pg_operator };
static NameData	ProcedureArgumentRelationNameData = { Name_pg_parg };
static NameData	PlatterRelationNameData = { Name_pg_proc };
static NameData	PlatterMapRelationNameData = { Name_pg_proc };
static NameData	ProcedureRelationNameData = { Name_pg_proc };
static NameData	Prs2PlansRelationNameData = { Name_pg_prs2plans };
static NameData	Prs2RuleRelationNameData = { Name_pg_prs2rule };
static NameData	Prs2StubRelationNameData = { Name_pg_prs2stub };
static NameData	RelationRelationNameData = { Name_pg_relation };
static NameData RewriteRelationNameData = { Name_pg_rewrite };
static NameData	ServerRelationNameData = { Name_pg_server };
static NameData	StatisticRelationNameData = { Name_pg_statistic };
static NameData TimeRelationNameData = { Name_pg_time };
static NameData	TypeRelationNameData = { Name_pg_type };
static NameData	UserRelationNameData = { Name_pg_user };
static NameData	VariableRelationNameData = { Name_pg_variable };
static NameData	VersionRelationNameData = { Name_pg_version };

Name	AggregateRelationName = &AggregateRelationNameData;
Name	AccessMethodRelationName = &AccessMethodRelationNameData;
Name	AccessMethodOperatorRelationName =
	&AccessMethodOperatorRelationNameData;
Name	AccessMethodProcedureRelationName =
	&AccessMethodProcedureRelationNameData;
Name	AttributeRelationName = &AttributeRelationNameData;
Name	DatabaseRelationName = &DatabaseRelationNameData;
Name	DefaultsRelationName = &DefaultsRelationNameData;
Name	DemonRelationName = &DemonRelationNameData;
Name	GroupRelationName = &GroupRelationNameData;
Name	IndexRelationName = &IndexRelationNameData;
Name	InheritProcedureRelationName = &InheritProcedureRelationNameData;
Name	InheritsRelationName = &InheritsRelationNameData;
Name	InheritancePrecidenceListRelationName =
	&InheritancePrecidenceListRelationNameData;
Name	LanguageRelationName = &LanguageRelationNameData;
Name    LargeObjectAssocRelationName = &LargeObjectAssocRelationNameData;
Name	ListenerRelationName = &ListenerRelationNameData;
Name    LogRelationName = &LogRelationNameData;
Name	MagicRelationName = &MagicRelationNameData;
Name    NamingRelationName = &NamingRelationNameData;
Name	OperatorClassRelationName = &OperatorClassRelationNameData;
Name	OperatorRelationName = &OperatorRelationNameData;
Name	ProcedureArgumentRelationName = &ProcedureArgumentRelationNameData;
Name	PlatterRelationName = &PlatterRelationNameData;
Name	PlatterMapRelationName = &PlatterMapRelationNameData;
Name	ProcedureRelationName = &ProcedureRelationNameData;
Name	Prs2PlansRelationName = &Prs2PlansRelationNameData;
Name	Prs2RuleRelationName = &Prs2RuleRelationNameData;
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

/* ----------------
 *	WARNING  WARNING  WARNING  WARNING  WARNING  WARNING
 *
 *	keep SharedSystemRelationNames[] in SORTED order!  A binary search
 *	is done on it in catalog.c!
 *
 *	XXX this is a serious hack which should be fixed -cim 1/26/90
 * ----------------
 */
Name SharedSystemRelationNames[10] = {
	&DatabaseRelationNameData,
	&DefaultsRelationNameData,
	&DemonRelationNameData,
	&GroupRelationNameData,
	&LogRelationNameData,
	&MagicRelationNameData,
	&ServerRelationNameData,
	&TimeRelationNameData,
	&UserRelationNameData,
	&VariableRelationNameData
};
