#! /bin/sh
# ----------------------------------------------------------------
#   FILE
#	Gen_creator.sh
#
#   DESCRIPTION
#       shell script which generates the Make, IMake and RMake
#	node creator functions, as well as the Out, Copy and
#	Equal functions.
#
#   NOTES
#	This process relies on the file $TREE/$OD/lib/H/slots
#	which is generated (earlier) by inherits.sh
#
#   IDENTIFICATION
# 	$Header$
# ----------------------------------------------------------------
SRC=$1
SLOTFILE=slots

EGREP=egrep
RM=rm
SED=sed

echo "/* ---------------------------------------------------------------- "
echo " * 	node file generated from $SRC"
echo " * "
echo " * 	this file has been generated by the Gen_accessors.sh"
echo " * 	and Gen_creator.sh scripts as part of the initial node"
echo " * 	generation process."
echo " * ---------------------------------------------------------------- "
echo " */"
echo "#include \"$SRC\""
echo " "
echo "#ifdef NO_NODE_CHECKING"
echo "#define NODEAssertArg(x)"
echo "#else"
echo "#define NODEAssertArg(x)	AssertArg(x)"
echo "#endif NO_NODE_CHECKING"
echo " "

# ----------------
#	strip trash from the input file and feed the result to awk..
# ----------------
$EGREP -v '(^#|^[ 	/]*\*|typedef|Defs|inherits)' < $SRC | \
$SED -e 's/;//' \
     -e '/\/\*/,/\*\//D' \
     -e 's/\\//' | \
awk '
# ----------------
#	initialize variables
# ----------------
BEGIN {
	SAVEFS = FS;
	ORS = " ";
	OFS = "";
	nc = 0;
}

# ----------------
#	first scan slots file and read in slot definitions
#	generated by inherits.sh
# ----------------
FILENAME != "-" && /{/, /}/ {
	if ($1 ~ /{/) {
		newclass = 1;
		next;
	}
	if (newclass == 1) {
		s = 1;
		slotclass = $2
		numslots[ slotclass ] = $1
		FS = ":";
		newclass = 0;
#		print FILENAME "--- " slotclass "\n" > "/dev/tty";
		next;
	}
	if ($1 ~ /}/) {
		FS = SAVEFS;
		next;
	}

	slots[ slotclass s ] = $1;
	slotdefs[ slotclass s++ ] = $2;
	next;
}

# ----------------
#	finished scanning slots file, now process stdin...
#
#	Here we have found the start of a new class definition.
#	Get the class name and begin generating the Make creator.
# ----------------
{ FS = SAVEFS; }
	
/class /,/{/ { 
	class = substr($2,2,length($2)-2);

#	print FILENAME "*** " class "\n" > "/dev/tty";

# ----
#   generate RInit initialization function
# ----
	print "\n/* ----------------";
	print "\n *    RInit", class, " - Raw initializer for ", class;
	print "\n * ";
	print "\n *    This function just initializes the internal";
	print "\n *    information in a node. ";
	print "\n * ----------------";
	print "\n */";
	print "\nextern void RInit", class, "();";
	print "\n";
	print "\nvoid \n", "RInit", class, "(p)";
	print "\nPointer p;";
	print "\n{";
	print "\n\textern void Out", class, "();";
	print "\n\textern bool Equal", class, "();";
	print "\n\textern bool Copy", class, "();";
	print "\n\n\t", class, " node = (", class, ") p;\n";
	print "\n\tnode->type = classTag(", class, ");";
	print "\n\tnode->outFunc = Out", class, ";";
	print "\n\tnode->equalFunc = Equal", class, ";";
	print "\n\tnode->copyFunc =  Copy", class, ";";
	print "\n\n\treturn;\n}\n";

# ----
#   generate the Make creator
# ----
	print "\n/* ----------------";
	print "\n *    Make creator function for ", class;
	print "\n * ";
	print "\n *    This function is in some sence \"broken\" because";
	print "\n *    it takes parameters for only this nodes slots and";
	print "\n *    leaves this nodes inherited slots uninitialized.";
	print "\n * ";
	print "\n *    This is here for backward compatibility with code";
	print "\n *    that relies on this behaviour.";
	print "\n * ----------------";
	print "\n */";
	print "\nextern ", class, " Make", class, "();";
	print "\n";
	print "\n", class, "\nMake", class, "(";
	i = 0;
}

# ----------------
#	Now process all the lines inbetween the { and the }
# ----------------
/{/,/}/	{
        if ($1 !~ /inherits/ && $1 !~ /struct/ && \
	    $1 !~ /class/ && $1 !~ /}/ ) {
	   	type[i] = $1;
		whole[i] = $0;
		decl[i++] = $2;
	}
	
	if ($1 ~ /struct/ ) {
		type[i] = $1+$2 ;
		whole[i] = $0;
		decl[i++] = substr($3,2,length($3)-1);
	}
}

# ----------------
#	Now generate the Make creator function, the Out, Equal
#	and Copy functions, and the IMake and RMake creators.
# ----------------
/}/ {
	for (x=0;x<i-1;x++)
		print decl[x], ","
	print decl[x]")\n"

	for (x=0;x<i;x++) 
		print whole[x],";\n"
		
	print "\n{"
	print "\n\t", class, " node = New_Node(", class, ");\n";
	print"\n\tRInit", class, "(node);\n";

	for (x=0;x<i;x++)
		print "\n\tset_", decl[x], "(node, ", decl[x], ");";

	print "\n\n\treturn(node);\n}\n";

# ----
#   generate Out function
# ----
	print "\n/* ----------------";
	print "\n *    Out function for ", class;
	print "\n * ----------------";
	print "\n */";
	print "\nextern void Out", class, "();";
	print "\n;";
	print "\nvoid \nOut", class, "(str, node)"
	print "\n\tStringInfo str;"
	print "\n\t", class, "\tnode;"
	print "\n{\n\tchar buf[100];"
	print "\n#ifdef\tOut", class, "Exists"
	print "\n\n\tappendStringInfo(str, \"#S(\");"
	print "\n\t_out", class, "(str, node);"
	print "\n\tappendStringInfo(str, \")\");"
	print "\n\n#else\t/* Out", class, "Exists */"
	print "\n\tsprintf(buf, \"#S(", class, " node at 0x%lx)\", node);\n"
	print "\n\tappendStringInfo(str, buf);"
	print "\n#endif\t/* Out", class, "Exists */"
	print "\n}\n"

# ----
#   generate Equal function
# ----
	print "\n/* ----------------";
	print "\n *    Equal function for ", class;
	print "\n * ----------------";
	print "\n */";
	print "\nextern bool Equal", class, "();";
	print "\n";
	print "\nbool\nEqual", class, "(a, b)";
	print "\n\t", class, "\ta, b;";
	print "\n{";
	print "\n#ifdef\tEqual", class, "Exists";
	print "\n\treturn ((bool) _equal", class, "(a, b));";
	print "\n\n#else\t/* Equal", class, "Exists */";
	print "\n\tprintf(\"Equal", class, " does not exist!\");\n";
	print "\n\treturn (false);";
	print "\n#endif\t/* Equal", class, "Exists */";
	print "\n}\n";

# ----
#   generate Copy function
# ----
	print "\n/* ----------------";
	print "\n *    Copy function for ", class;
	print "\n * ----------------";
	print "\n */";
	print "\nextern bool Copy", class, "();";
	print "\n";
	print "\nbool\nCopy", class, "(from, to, alloc)";
	print "\n\t", class, "\tfrom;";
	print "\n\t", class, "\t*to;\t/* return */";
	print "\n\tchar *\t(*alloc)();";
	print "\n{";
	print "\n#ifdef\tCopy", class, "Exists";
	print "\n\treturn ((bool) _copy", class, "(from, to, alloc));";
	print "\n\n#else\t/* Copy", class, "Exists */";
	print "\n\tprintf(\"Copy", class, " does not exist!\");\n";
	print "\n\treturn (false);";
	print "\n#endif\t/* Copy", class, "Exists */";
	print "\n}\n";

# ----
#   generate IMake creator
# ----
	print "\n/* ----------------";
	print "\n *    IMake", class, " - Inherited Make creator for ", class;
	print "\n * ";
	print "\n *    This creator function takes a parameter";
	print "\n *    for each slot in this class and each slot";
	print "\n *    in all inherited classes.";
	print "\n * ----------------";
	print "\n */";
	print "\nextern ", class, " IMake", class, "();";
	print "\n";
	print "\n", class, "\n", "IMake", class, "(";

	n = numslots[ class ];
	if (n > 0) {
		for (s=1; s<=n-1; s++)
			print slots[ class s ], ",";
		print slots[ class s ], ")\n";

		for (s=1; s<=n; s++)
			print slotdefs[ class s ], ";\n";
	} else {
		print ")\n";
	}

	print "\n{";
	print "\n\t", class, " node = New_Node(", class, ");\n";
	print "\n\tRInit", class, "(node);\n";
	
	if (n > 0) {
		for (s=1; s<=n ; s++) {
			slot = slots[ class s ];
			print "\n\tset_", slot, "(node, ", slot, ");";
		}
	}
	print "\n\n\treturn(node);\n}\n";

# ----
#   generate RMake creator
# ----
	print "\n/* ----------------";
	print "\n *    RMake", class, " - Raw Make creator for ", class;
	print "\n * ";
	print "\n *    This creator function does not initialize";
	print "\n *    any of its slots..  This is left up to the";
	print "\n *    calling routine.";
	print "\n * ----------------";
	print "\n */";
	print "\nextern ", class, " RMake", class, "();";
	print "\n";
	print "\n", class, "\n", "RMake", class, "()";
	print "\n{";
	print "\n\t", class, " node = New_Node(", class, ");\n";
	print "\n\tRInit", class, "(node);";
	print "\n\n\treturn(node);\n}\n";
}

# ----------------
#	thats all folks
# ----------------
END {
	ORS="\n"
	print "\n/* end-of-file */\n"
}
' $SLOTFILE -
