/*
 * catname.h --
 *	POSTGRES system catalog relation name definitions.
 */

#ifndef	CatNameIncluded		/* Include this file only once */
#define CatNameIncluded	1

/*
 * Identification:
 */
#define CATNAME_H	"$Header$"

#include "tmp/postgres.h"

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
extern Name	RewriteRelationName;
extern Name	ServerRelationName;
extern Name	StatisticRelationName;
extern Name	TimeRelationName;
extern Name	TypeRelationName;
extern Name	UserRelationName;
extern Name	VariableRelationName;
extern Name	VersionRelationName;
extern Name	Prs2RuleRelationName;
extern Name	Prs2PlansRelationName;
extern Name	Prs2StubRelationName;

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
