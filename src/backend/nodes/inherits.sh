#! /bin/sh
#
# $Header$
#
TMPDIR=${TMPDIR-/tmp}
INHFILE=$TMPDIR/inh.$$
TAGFILE=$OD/tags.h
TAGTEMP=$OD/tags.temp
OUTFILE=inh.c
NODEFILES="nodes.h plannodes.h primnodes.h \
relation.h execnodes.h mnodes.h lnodes.h pg_lisp.h"
#
# Generate the initial inheritance graph
#
egrep -h '^class' $NODEFILES | \
sed \
 -e 's/^class (\([A-Za-z]*\))/\1/' \
 -e 's/ public (\([A-Za-z]*\))/ \1/' \
 -e 's/[ {}]*$//' > $INHFILE
#
# Assign tags to the classes
#
cat > $TAGTEMP << 'EOF'
/*
 * node tag header file
 *
 * Based on: $Header$
 */

EOF
awk 'BEGIN { i = -1 }\
     { printf("#define T_%s %d\n", $1, ++i) }' $INHFILE >> $TAGTEMP
#
#
#
cat > $OUTFILE << 'EOF'
/*
 * inheritance graph file
 *
 * Based on: $Header$
 */

#include "pg_lisp.h"
#include "c.h"
#include "nodes.h"
EOF
echo '#include' \"$TAGFILE\" >> $OUTFILE
cat >> $OUTFILE << 'EOF'

struct nodeinfo {
	char	*ni_name;
	TypeId	ni_id;
	TypeId	ni_parent;
};
static struct nodeinfo _NodeInfo[] = {
EOF
awk '{ if ($2 == "") { $2 = "Node" };\
       printf("	{ \"%s\", T_%s, T_%s },\n", $1, $1, $2) }' \
      $INHFILE >> $OUTFILE
cat >> $OUTFILE << 'EOF'
	{ "INVALID", 0, 0 }
};

TypeId	_InvalidTypeId = (TypeId) (lengthof(_NodeInfo) - 1);

bool	_NodeInfoTrace = false;

bool
NodeIsType(thisNode, tag)
	Node	thisNode;
	TypeId	tag;
{
	register TypeId	i;

	Assert(NodeIsValid(thisNode));
	Assert(TypeIdIsValid(NodeType(thisNode)));
	Assert(TypeIdIsValid(tag));

	for (i = NodeType(thisNode);
	     i != tag && i != T_Node;
	     i = _NodeInfo[i].ni_parent)
		if (_NodeInfoTrace)
			printf("NodeIsType: %s IsA %s?\n",
			       _NodeInfo[i].ni_name, _NodeInfo[tag].ni_name);
	if (_NodeInfoTrace) {
		printf("NodeIsType: %s IsA %s?\n",
		       _NodeInfo[i].ni_name, _NodeInfo[tag].ni_name);
		printf("NodeIsType: %s IsA %s returns %s\n",
		       _NodeInfo[NodeType(thisNode)].ni_name,
		       _NodeInfo[tag].ni_name,
		       (i == tag) ? "true" : "false");
	}
	return((bool) (i == tag));
}

void
Dump_NodeInfo()
{
	register TypeId	i;

	printf("%16.16s%16.16s%16.16s\n", 
	       "NODE NAME:", "NODE TAG:", "PARENT NODE:");
	for (i = 0; i < _InvalidTypeId; ++i)
		printf("%16.16s%16.1d%16.16s\n",
		       _NodeInfo[i].ni_name,
		       _NodeInfo[i].ni_id, 
		       _NodeInfo[_NodeInfo[i].ni_parent].ni_name);
}
bool
NodeTagIsValid(tag)
	NodeTag	tag;
{
	return ((bool)(tag < _InvalidTypeId));
}

NodeTag
NodeGetTag(node)
	Node	node;
{
	Assert(NodeIsValid(node));

	return (node->type);
}

void
NodeSetTag(node, tag)
	Node	node;
	NodeTag	tag;
{
	Assert(PointerIsValid(node));
	Assert(NodeTagIsValid(tag));

	node->type = tag;
}

bool
NodeHasTag(node, tag)
	Node	node;
	NodeTag	tag;
{
	return ((bool)(NodeGetTag(node) == tag));
}

EOF
rm -f $INHFILE

if [ -r $TAGFILE ]; then 
	if cmp -s $TAGFILE $TAGTEMP ; then 
		echo "tags.h unchanged";
	else
		mv $TAGTEMP $TAGFILE; 
		echo "tags.h has changed; remake cinterface.a";
	fi
else
	mv $TAGTEMP $TAGFILE;
fi
exit 0
