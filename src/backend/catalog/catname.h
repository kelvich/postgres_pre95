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

#ifndef C_H
#include "c.h"
#endif

#ifndef	NAME_H
# include "name.h"
#endif

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

#define CATNAME_SYMBOLS \
	ExternDecl(AggregateRelationName, "_AggregateRelationName"), \
	ExternDecl(AccessMethodRelationName, "_AccessMethodRelationName"), \
	ExternDecl(AccessMethodOperatorRelationName, "_AccessMethodOperatorRelationName"), \
	ExternDecl(AttributeRelationName, "_AttributeRelationName"), \
	ExternDecl(DatabaseRelationName, "_DatabaseRelationName"), \
	ExternDecl(DefaultsRelationName, "_DefaultsRelationName"), \
	ExternDecl(DemonRelationName, "_DemonRelationName"), \
	ExternDecl(IndexRelationName, "_IndexRelationName"), \
	ExternDecl(InheritProcedureRelationName, "_InheritProcedureRelationName"), \
	ExternDecl(InheritsRelationName, "_InheritsRelationName"), \
	ExternDecl(InheritancePrecidenceListRelationName, "_InheritancePrecidenceListRelationName"), \
	ExternDecl(LanguageRelationName, "_LanguageRelationName"), \
	ExternDecl(LogRelationName, "_LogRelationName"), \
	ExternDecl(MagicRelationName, "_MagicRelationName"), \
	ExternDecl(OperatorClassRelationName, "_OperatorClassRelationName"), \
	ExternDecl(OperatorRelationName, "_OperatorRelationName"), \
	ExternDecl(ProcedureArgumentRelationName, "_ProcedureArgumentRelationName"), \
	ExternDecl(ProcedureRelationName, "_ProcedureRelationName"), \
	ExternDecl(RelationRelationName, "_RelationRelationName"), \
	ExternDecl(RulePlansRelationName, "_RulePlansRelationName"), \
	ExternDecl(RuleRelationName, "_RuleRelationName"), \
	ExternDecl(ServerRelationName, "_ServerRelationName"), \
	ExternDecl(StatisticRelationName, "_StatisticRelationName"), \
	ExternDecl(TimeRelationName, "_TimeRelationName"), \
	ExternDecl(TypeRelationName, "_TypeRelationName"), \
	ExternDecl(UserRelationName, "_UserRelationName"), \
	ExternDecl(VariableRelationName, "_VariableRelationName"), \
	ExternDecl(VersionRelationName, "_VersionRelationName"), \
	SymbolDecl(NameIsSystemRelationName, "_NameIsSystemRelationName"), \
	SymbolDecl(NameIsSharedSystemRelationName, "_NameIsSharedSystemRelationName")

#endif	/* !defined(CatNameIncluded) */
