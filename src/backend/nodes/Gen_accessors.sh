#! /bin/sh
#
#	Gen_accessors -- Create accessor functions for nodes.
#
#	$Header$
#
####
# initial definitions
####
CAT=/bin/cat
CB=/usr/bin/cb
CPP=/lib/cpp
EGREP=/usr/bin/egrep
RM=/bin/rm
SED=/usr/bin/sed.old
SEDTMP=/tmp/sedtmp.$$
SED2=/tmp/sed2.$$
CTMP1=/tmp/ctmp1.$$
HTMP1=/tmp/htmp1.$$
SRC=$1
####
# set up some necessary input files
####
$CAT > $SEDTMP << 'EOF'
/\/\*/,/\*\//D
/[ 	][ 	]*/s// /g
/^class/ {
	i\
#undef XXXXXX
	s/class (\([A-Za-z_][A-Za-z0-9_]*\)).*/#define XXXXXX \1/
}
s/^ \([A-Za-z_][A-Za-z0-9_]*\) \([A-Za-z_][A-Za-z0-9_]*\)[; \\]*$/ACCESSORS(XXXXXX,\2,\1)/
s/^ \([A-Za-z_][A-Za-z0-9_ ]*\) \(\*\)\([A-Za-z_][A-Za-z0-9_]*\)[; \\]*$/ACCESSORS(XXXXXX,\3,\1 \2)/
EOF

$CAT > $SED2 << 'EOF'
/\/\*/,/\*\//D
/^class/ {
	i\
#undef XXXXXX
	s/class (\([A-Za-z_][A-Za-z0-9_]*\)).*/#define XXXXXX \1/
}
s/^[ 	]*\([A-Za-z_][A-Za-z0-9_]*\)[ 	][ 	]*\([A-Za-z_][A-Za-z0-9_]*\)[; \\]*$/ACCESSORS(XXXXXX,\2,\1)/
s/^[ 	]*\([A-Za-z_][A-Za-z0-9_ ]*\)[	][	]*\(\*\)\([A-Za-z_][A-Za-z0-9_]*\)[; \\]*$/ACCESSORS(XXXXXX,\3,\1 \2)/
EOF

$CAT > $HTMP1 << 'EOF'
#define CppIdentity(a)a
#define CppConcat(a,b)CppIdentity(a)b

#define	ACCESSORS(_nodetype_,_fieldname_,_fieldtype_) \
extern void \
CppConcat(set_,_fieldname_) ARGS((_nodetype_ node, _fieldtype_ value));\
extern _fieldtype_ \
CppConcat(get_,_fieldname_) ARGS((_nodetype_ node)); \
EOF

$CAT > $CTMP1 << 'EOF'
#define CppIdentity(a)a
#define CppConcat(a,b)CppIdentity(a)b

#define	ACCESSORS(_nodetype_,_fieldname_,_fieldtype_) \
void \
CppConcat(set_,_fieldname_)(node, value) \
	_nodetype_	node; \
	_fieldtype_	value; \
{ \
	AssertArg(IsA(node,_nodetype_)); \
	node->_fieldname_ = value; \
} \
_fieldtype_ \
CppConcat(get_,_fieldname_)(node) \
	_nodetype_	node; \
{ \
	AssertArg(IsA(node,_nodetype_)); \
	return(node->_fieldname_); \
} \
EOF
####
# do the actual work
####
echo "#include \"$SRC\""
$EGREP -v '(^#|^[ 	/]*\*|typedef|Defs|inherits|})' < $SRC | \
$SED -f $SEDTMP | \
$CAT $CTMP1 - | \
$CPP -P | \
$SED 's/T_ /T_/' | \
$CB | \
$EGREP -v '^$'
#$EGREP -v '^$' > $SRC.a.c

#$EGREP -v '(^#|^[ 	/]*\*|typedef|Defs|inherits|})' < $SRC.h | \
#$SED -f $SED2 | \
#$CAT $HTMP1 - | \
#$CPP -P | \
#$CB | \
#$EGREP -v '^$' > $SRC.a.h

$RM -f $SEDTMP $SED2 $HTMP1 $CTMP1
