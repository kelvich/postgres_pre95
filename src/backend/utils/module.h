/* ----------------------------------------------------------------
 *   FILE
 *	module.h
 *
 *   DESCRIPTION
 *	this file contains general "module" stuff  that used to be
 *	spread out between the following files:
 *
 *	enbl.h			module enable stuff
 *	trace.h			module trace stuff
 *
 *   NOTES
 *	some more stuff may be moved here.
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */
#ifndef ModuleHIncluded
#define ModuleHIncluded 1	/* include once only */

#include "tmp/c.h"
#include "tmp/simplelists.h"

/* ----------------
 *	enbl.h
 * ----------------
 */
/*
 * enbl.h --
 *	POSTGRES module enable and disable support code.
 *
 * Identification:
 *	$Header$
 */

#ifndef	EnblIncluded		/* Include this file only once */
#define EnblIncluded	1

#define ENBL_H	"$Header$"

/*
 * BypassEnable --
 *	False iff enable/disable processing is required given on and "*countP."
 *
 * Note:
 *	As a side-effect, *countP is modified.  It should be 0 initially.
 *
 * Exceptions:
 *	BadState if called with pointer to value 0 and false.
 *	BadArg if "countP" is invalid pointer.
 *	BadArg if on is invalid.
 */
extern
bool
BypassEnable ARGS((
	Count	*enableCountInOutP,
	bool	on
));

#endif	/* !defined(EnblIncluded) */

/* ----------------
 *	trace.h
 * ----------------
 */
/*
 *  TRACE.H
 *
 *  $Header$
 *
 *  Trace Hierarchy:
 *	The TraceTree[] array in trace.c holds the global trace hierarchy.
 *	Up to two parents may be specified for every trace name.  Each
 *	source module using the trace functions may declare any number
 *	of trace variables optionally giving them zero, one, or two parents:
 *  
 *	    DeclareTrace(ParserLex)
 *	    DeclareTraceParent(ParserLoops, Root)
 *	    DeclareTraceParents(ParserEval, Root, Parser)
 *
 *	The first declarations creates the trace variable 'ParserLex'
 *	which has no parents.  The second creates another trace
 *	variable 'ParserLoops' who has Root as a parent (Root is declared
 *	in the TraceTree[] array in TRACE.C).  The third declares a
 *	third trace variable with two parents.
 *
 *	Giving a trace variable one or more parents means that if tracing
 *	is turned on for either parent, it is automatically turned on for
 *	the variable.  This is recursive... a tree of trace variables may
 *	be specified in the TraceTree[] array in TRACE.C
 *
 *	One can declare a limited two level tree locally (private to the
 *	module).  e.g.:
 *	
 *	    DeclareTrace(Foo)
 *	    DeclareTraceParent(Bar1, Foo)
 *	    DeclareTraceParent(Bar2, Foo)
 *
 *	Foo may not have any parents.
 *
 *	Trace variables are enabled by setting enviroment variables to
 *	the trace level desired.  TRACE_<tracename>.  For example:
 *
 *	    % setenv TRACE_Foo 1
 *
 *	If the enviroment variable exists but has no value or is 0, i.e.:
 *
 *	    % setenv TRACE_Foo		or
 *	    % setenv TRACE_Foo 0
 *
 *	The trace level is set to 1.  Any number of TRACE_ enviroment
 *	variables may be set, enabling those specific trace variables.
 *
 *	Trace variables may be tested with the Traced() macro.
 *	Trace messages may be generated with TraceMsg*() macros.  Since
 *	a level is associated with each trace variable, only messages at
 *	or below the current trace level will be output.  
 *
 *	Messages at trace level 0 are always output.  The TraceMsg*() calls
 *	output messages and level 1 while the TraceMsgAt*() calls output
 *	messages at the specified level.
 *
 *	Variations of the Declare*() macros exist allowing you to 
 *	assign a stream other than the default to the variable.  The
 *	stream directs where the output will go (see enum below).
 *
 *	SEE TEST PROGRAM FOR EXAMPLES (test/testtrace.c)
 */

#ifndef TRACE_H
#define TRACE_H

#define TraceBlock struct _TraceBlock
#define TraceStack struct _TraceStack

#define TBF_INITIALIZED		0x01
#define TBF_FAULT		0x02

typedef enum TraceStream { 
	DefaultStream, LogStream, InstallationStream, DBStream, ConsoleStream,
	FrontendStream, PostmasterStream, AuxStream
} TraceStream;

TraceBlock {
    char   *Module;	/* trace module			*/
    char   *Name;	/* trace variable name	        */
    char   *Parent1;	/* name of parent / NULL        */
    char   *Parent2;	/* name of parent / NULL        */
    long    Level;	/* trace level, 0 = disabled	*/
    TraceStream Stream;
    uint32  Flags;	/* trace flags		        */
    SLNode  Node;	/* link node for trace.c	*/
};

TraceStack {
    TraceBlock *Tb;
    int    LineNo;	/* line in module		*/
    char   *Text;	/* display this text		*/
    SLNode  Node;	/* link node for trace.c	*/
};

#define TNAME(name)	CppConcat(Trace_,name)

#define Traced(name)		\
	(TraceInitialize(&TNAME(name)), (TNAME(name).Level > 0))

#define GetTraceLevel(name) 	\
	(TraceInitialize(&TNAME(name)), (TNAME(name).Level))

#define DeclareTraceTo(stream,name) \
	static TraceBlock TNAME(name) = \
	{ __FILE__, CppAsString(name) }

#define DeclareTraceParentTo(stream,name,parent)	\
	static TraceBlock TNAME(name) = \
	{ __FILE__, CppAsString(name),CppAsString(parent) }

#define DeclareTraceParentsTo(stream,name,p1,p2)  	\
	static TraceBlock TNAME(name) = \
	{ __FILE__, CppAsString(name), CppAsString(p1), CppAsString(p2) }

#define DeclareTrace(name)		\
	DeclareTraceTo(DefaultStream,name)

#define DeclareTraceParent(name,parent)	\
	DeclareTraceParentTo(DefaultStream,name,parent)

#define DeclareTraceParents(name,p1,p2) \
	DeclareTraceParentsTo(DefaultStream,name,p1,p2)


#define TraceMsg0(tb,mc)    		_TraceMsg(&TNAME(tb),1,0,mc)
#define TraceMsg1(tb,mc,m1)    		_TraceMsg(&TNAME(tb),1,1,mc,m1)
#define TraceMsg2(tb,mc,m1,m2)    	_TraceMsg(&TNAME(tb),1,2,mc,m1,m2)

#define TraceMsgSub0(tb,lv,mc)		_TraceMsg(&TNAME(tb),lv,0,mc)
#define TraceMsgSub1(tb,lv,mc,m1)	_TraceMsg(&TNAME(tb),lv,1,mc,m1)
#define TraceMsgSub2(tb,lv,mc,m1,m2)  	_TraceMsg(&TNAME(tb),lv,2,mc,m1,m2)

#define TraceOn(tb,level)		SetTraceLevel(TNAME(tb).Name,level)
#define TraceOff(tb)			SetTraceLevel(TNAME(tb).Name,0)

/*
 *  Subroutine Tracing
 */

#define TracePush(tb, str)		_TracePush(&TNAME(tb), __LINE__, str)
#define TracePop(tb)			_TracePop(&TNAME(tb), __LINE__, "")
#define TracePop1(tb, str)		_TracePop(&TNAME(tb), __LINE__, str)
#define TraceReset(tb, str)		_TraceReset(&TNAME(tb), __LINE__, str)
#define TraceDump()			_TraceDump()

#endif TRACE_H


/* ----------------
 *	end of module.h
 * ----------------
 */
#endif ModuleHIncluded
