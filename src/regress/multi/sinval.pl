#!/usr/sww/bin/perl
#
# sinval.pl [-d database]
#
# $Header$
#

#-----------------------------------------------------------------------
# the standard "#! didn't work" trick, straight from the man page.
# must precede all perl code.
#-----------------------------------------------------------------------
eval '(exit $?0)' && eval 'exec /usr/sww/bin/perl -S $0 ${1+"$@"}'
    & eval 'exec /usr/sww/bin/perl -S $0 $argv:q'
    if 0;

require 'getopts.pl';           # from the perl distribution

($program = $0) =~ s%.*/%%;

if (&Getopts('d:') != 1) {
    do &Usage();
}

$dbname = $ENV{'USER'};
$dbname = $opt_d if ($opt_d ne '');
do &Usage() if ($dbname eq '');

#
# 0 is the idle monitor
# 1 is the busy monitor
#
foreach $i (0..1) {
    $outfile = "sinval$$.$i";
    $mon_fh[$i] = "mon" . $i;
    open($mon_fh[$i], "| monitor $dbname >$outfile 2>&1") ||
	die("$program: monitor startup failed\n");
    select($mon_fh[$i]);
    $| = 1;
    select(STDOUT);
}

#
# the sinval buffer is hardcoded at 1000 messages
# we should be able to fill it with 1000 catalog updates
# each "create" creates
#	1 class
#	2 types
#	13 system attributes
#	n user attributs
# so we'll create 4 user attributes for a total of an even 20 rows
# updated per create.  that's 50 iterations and we'll do 100 to be
# sure.
#
$max_iters = 100;
$relname = "testrel";

#
# make sure the idle monitor is alive and thinking
#
$which = $mon_fh[0];
print $which "retrieve (x=1)\\g\n";

#
# go for it
#
$destroy = "destroy $relname\\g\n";
$create = "create $relname (a=int4,b=int4,c=int4,d=int4)\\g\n";

$which = $mon_fh[1];
for ($i = 0; $i < $max_iters; ++$i) {
    print $which $destroy;
    print $which $create;
}

#
# wait for the busy process to complete
#
close($mon_fh[1]);

#
# make sure the idle monitor resets its caches
#
$which = $mon_fh[0];
print $which "retrieve (x=1)\\g\n";

#
# wait for the idle process to complete
#
close($mon_fh[0]);

exit(0);

#
# Usage
#
# exit with a helpful usage message.
#
sub Usage {
    print "Usage: $program [-d database]\n";
    exit(1);
}
