#! /bin/sh
# ----------------------------------------------------------------
#   FILE
#	genbki.sh
#
#   DESCRIPTION
#       shell script which generates .bki files from specially
#	formatted .h files.  These .bki files are used to initialize
#	the postgres template database.
#
#   NOTES
#
#   IDENTIFICATION
# 	$Header$
# ----------------------------------------------------------------

# ----------------
# 	collect nodefiles
# ----------------
SYSFILES=''
x=1
numargs=$#
while test $x -le $numargs ; do
    SYSFILES="$SYSFILES $1"
    x=`expr $x + 1`
    shift
done

# ----------------
# 	strip comments and trash from .h before we generate
#	the .bki file...
# ----------------
#
cat $SYSFILES | \
sed -e '/^#/d' \
    -e 's/\/\*.*\*\///g' \
    -e 's/;[ 	]*$//g' | \
awk '
# ----------------
#	now use awk to process remaining .h file..
#
#	nc is the number of catalogs
#	inside is a variable set to 1 when we are scanning the
#	   contents of a catalog definition.
# ----------------
BEGIN {
	inside = 0;
	bootstrap = 0;
	nc = 0;
}

# ----------------
#	DATA() statements should get passed right through after
#	stripping off the DATA( and the ) on the end.
# ----------------
/^DATA\(/ {
	data = substr($0, 6, length($0) - 6);
	print data;
	next;
}

# ----------------
#	CATALOG() definitions take some more work.
# ----------------
/^CATALOG\(/ { 
# ----
#  end any prior catalog definitions before starting a new one..
# ----
	if (nc > 0) {
		print "show";
		print "close " catalog;
	}

# ----
#  get the name of the new catalog
# ----
	pos = index($1,")");
	catalog = substr($1,9,pos-9); 

	if ($0 ~ /BOOTSTRAP/) {
		bootstrap = 1;
	}

        i = 1;
	inside = 1;
        nc++;
	next;
}

# ----------------
#	process the contents of the catalog definition
#
#	attname[ x ] contains the attribute name for attribute x
#	atttype[ x ] contains the attribute type fot attribute x
# ----------------
inside == 1 {
# ----
#  ignore a leading brace line..
# ----
        if ($1 ~ /{/)
		next;

# ----
#  if this is the last line, then output the bki catalog stuff.
# ----
	if ($1 ~ /}/) {
		if (bootstrap) {
			print "create bootstrap " catalog;
		} else {
			print "create " catalog;
		}
		print "\t(";

		for (j=1; j<i-1; j++) {
			print "\t " attname[ j ] " = " atttype[ j ] " ,";
		}
		print "\t " attname[ j ] " = " atttype[ j ] ;
		print "\t)";

		if (! bootstrap) {
			print "open " catalog;
		}

		i = 1;
		inside = 0;
		bootstrap = 0;
		next;
	}

# ----
#  if we are inside the catalog definition, then keep sucking up
#  attibute names and types
# ----
	atttype[ i ] = $1;
	attname[ i ] = $2;
	i++;
	next;
}

END {
	print "show";
	print "close " catalog;
}
'

# ----------------
#	all done
# ----------------
exit 0
