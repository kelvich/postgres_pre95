#
# lookcook.awk
#
# cat each monitor file from pgcook into this script:
#	% cat pgcook12345.[0-9]* | awk -f lookcook.awk
#
# $Header$
#
BEGIN {
	append = 0;
	replace = 0;
	retrieve = 0;
	last = "";
}
{
	if ($0 ~ /deadlock/) {
		if (last ~ /append/) {
			append += 1;
		} else if (last ~ /retrieve/) {
			retrieve += 1;
		} else if (last ~ /replace/) {
			replace += 1;
		}
	}
	last = $0;
}
END {
	print "appends=" append;
	print "replaces=" replace;
	print "retrieve=" retrieve;
}
