#! /bin/sh
#
#	Gen_accessors -- Create accessor functions for nodes.
#
#	$Header$
#

# ----------------
# 	initial definitions
# ----------------
CAT=/bin/cat
CB=/usr/bin/cb
CPP=/lib/cpp
EGREP=/usr/bin/egrep
RM=/bin/rm
SED=/bin/sed
SEDTMP=/tmp/sedtmp.$$
CTMP1=/tmp/ctmp1.$$
SRC=$1

# ----------------
# 	set up some necessary input files
# ----------------
$CAT > $SEDTMP << 'EOF'
/\/\*/,/\*\//D
/[ 	][ 	]*/s// /g
/^class/ {
	i\
#undef XXXXXX
	s/class (\([A-Za-z_][A-Za-z0-9_]*\)).*/#define XXXXXX \1/
}
s/^ \([A-Za-z_][A-Za-z0-9_]*\) \([A-Za-z_][A-Za-z0-9_]*\)[; \\]*$/GETACCESSOR(XXXXXX,\2,\1)\
SETACCESSOR(XXXXXX,\2,\1)/
s/^ \([A-Za-z_][A-Za-z0-9_ ]*\) \(\*\)\([A-Za-z_][A-Za-z0-9_]*\)[; \\]*$/GETACCESSOR(XXXXXX,\3,\1 \2)\
SETACCESSOR(XXXXXX,\3,\1 \2)/
EOF

# ----------------
#	set up CTMP1
# ----------------
$CAT > $CTMP1 << 'EOF'
#define _SHARP_	#	/* Ansi braindeath - no way to quote # in macro replacement */
#define CppIdentity(a)a
#define CppConcat(a,b)CppIdentity(a)b

#define	SETACCESSOR(_nodetype_,_fieldname_,_fieldtype_) \
_SHARP_ define \
CppConcat(set_,_fieldname_)(node, value) \
    { \
	NODEAssertArg(IsA(node,_nodetype_)); \
	(node)->_fieldname_ = (value); \
    }

#define	GETACCESSOR(_nodetype_,_fieldname_,_fieldtype_) \
_SHARP_ define CppConcat(get_,_fieldname_)(node) ((node)->_fieldname_)
EOF

# ----------------
# 	do the actual work
# ----------------

echo "/* ---------------------------------------------------------------- "
echo " * 	node file generated from $SRC"
echo " * "
echo " * 	this file has been generated by the Gen_accessors.sh"
echo " * 	and Gen_creator.sh scripts as part of the initial node"
echo " * 	generation process."
echo " * ---------------------------------------------------------------- "
echo " */"
echo " "
echo "#ifdef NO_NODE_CHECKING"
echo "#define NODEAssertArg(x)"
echo "#else"
echo "#define NODEAssertArg(x)	AssertArg(x)"
echo "#endif NO_NODE_CHECKING"
echo " "

$EGREP -v '(^#|^[ 	/]*\*|typedef|Defs|inherits|})' < $SRC | \
$SED -f $SEDTMP | \
$CAT $CTMP1 - | \
$CPP -P | \
$SED 's/T_ /T_/' | \
$CB | \
$EGREP -v '^$'

$RM -f $SEDTMP $HTMP1 $CTMP1
