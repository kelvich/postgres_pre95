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
awk 'BEGIN 		{ ORS = " "; OFS = "" }
/class/,/{/ 	{ class = substr($2,2,length($2)-2) ; print "\n",class, \
		  "\nMake",class,"("; i = 0}
/{/,/}/ 	{
	          if ($1 !~ /inherits/ && $1 !~ /class/ && $1 !~ /}/ ) {
	   		type[i] = $1 ;
			whole[i] = $0;
			decl[i++] = $2;
		  }
		}
/}/ 		{ for (x=0;x<i-1;x++)
			print decl[x],","
		  print decl[x]")\n"
		  for (x=0;x<i;x++) 
			print whole[x],";\n"
		  print "\n{\n\t",class," node = New_Node(",class,");\n"
		  for (x=0;x<i;x++)
			print "\n\tset_",decl[x],"(",decl[x],");"
		  print "\n\treturn(node);\n}"
		}

END { print "\n/* end-of-file */\n" }' 


