#! /bin/sh
####
# initial definitions
####

CAT=/bin/cat
CB=/usr/bin/cb
CPP=/lib/cpp
EGREP=/usr/bin/egrep
RM=/bin/rm
SED=/bin/sed
SEDTMP=/tmp/sedtmp.$$
SED2=/tmp/sed2.$$
CTMP1=/tmp/ctmp1.$$
HTMP1=/tmp/htmp1.$$
SRC=$1

###

$EGREP -v '(^#|^[ 	/]*\*|typedef|Defs|inherits)' < $SRC | \
$SED 's/;//' | \
$SED '/\/\*/,/\*\//D' | \
$SED 's/\\//' | \
awk 'BEGIN 		{ ORS = " "; OFS = "" }
/class /,/{/ 	{ class = substr($2,2,length($2)-2) ; print "\n",class, \
		  "\nMake",class,"("; i = 0}
/{/,/}/ 	{
	          if ($1 !~ /inherits/ && $1 !~ /struct/ && $1 !~ /class/ && $1 !~ /}/ ) {
	   		type[i] = $1 ;
			whole[i] = $0;
			decl[i++] = $2;
		  }
		  if ($1 ~ /struct/ ) {
	   		type[i] = $1+$2 ;
			whole[i] = $0;
			decl[i++] = substr($3,2,length($3)-1);
		  }
		}
/}/ 		{ for (x=0;x<i-1;x++)
			print decl[x],","
		  print decl[x]")\n"
		  for (x=0;x<i;x++) 
			print whole[x],";\n"
		  print "\n{"
		  print "\n\textern void Print",class,"();"
		  print "\n\textern bool Equal",class,"();"
		  print "\n\n\t",class," node = New_Node(",class,");\n"
		  for (x=0;x<i;x++)
			print "\n\tset_",decl[x],"( node ,",decl[x],");"
		  print"\n\tnode->printFunc = Print",class,";"
		  print"\n\tnode->equalFunc = Equal",class,";"
		  print "\n\treturn(node);\n}"
		  print "\nvoid \nPrint",class,"(node)\n\t",class,"\tnode;"
		  print "\n{"
		  print "\n#ifdef\tPrint",class,"Exists"
		  print "\n\n\tprintf(\"#S(\");"
		  print "\n\t_print",class,"(node);"
		  print "\n\tprintf(\")\");"
		  print "\n\n#else\t/* Print",class,"Exists */"
		  print "\n\tprintf(\"#S(",class," node at 0x%lx)\", node);\n"
		  print "\n#endif\t/* Print",class,"Exists */"
		  print "\n}"
		  print "\nbool\nEqual",class,"(a, b)\n\t",class,"\ta, b;"
		  print "\n{"
		  print "\n#ifdef\tEqual",class,"Exists"
		  print "\n\treturn ((bool) _equal",class,"(a, b));"
		  print "\n\n#else\t/* Equal",class,"Exists */"
		  print "\n\tprintf(\"Equal",class," does not exist!\");\n"
		  print "\n\treturn (false);"
		  print "\n#endif\t/* Equal",class,"Exists */"
		  print "\n}"
		}

END { print "\n/* end-of-file */\n" }' 


