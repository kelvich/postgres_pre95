#!/usr/sww/bin/perl
#
# pgcook [-d database] [-m #-monitors] [-n n-iterations] [-D] [-S]
#
# a summary of all queries is dumped into pgcook$$.out
# a summary of queries and output per-monitor is dumped into pgcook$$.i
#  for monitor i
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

if (&Getopts('Dd:m:n:') != 1) {
    do &Usage();
}

$dbname = $ENV{'USER'};
$dbname = $opt_d if ($opt_d ne '');
do &Usage() if ($dbname eq '');

$n_mon = 3;
$n_mon = $opt_m if ($opt_m ne '');
do &Usage() if ($n_mon < 1);

$max_iters = 1000;
$max_iters = $opt_n if ($opt_n ne '');
do &Usage() if ($max_iters < 1);

$use_deadlock = 0;
$use_deadlock = $opt_D if ($opt_D ne '');
do &Usage() if ($use_deadlock != 0 && $use_deadlock != 1);

print "$program: doing $max_iters iterations using $n_mon monitors on database $dbname\n";
print "\talso testing:\n";
print "\tdeadlock handling\n" if ($use_deadlock == 1);

$n_mon = $n_mon - 1;		# now the high monitor number

srand;
$outfile = "pgcook$$.out";
open(STDOUT, ">$outfile") || die "Can't redirect stdout";
open(STDERR, ">&STDOUT") || die "Can't dup stdout";
select(STDERR); $| = 1;
select(STDOUT); $| = 1;
foreach $i (0..$n_mon) {
    $mon_fh[$i] = "mon" . $i;
    $outfile = "pgcook$$.$i";
    open($mon_fh[$i], "| monitor $dbname > $outfile 2>&1") ||
	die("$program: monitor startup failed\n");
    select($mon_fh[$i]);
    $| = 1;
    select(STDOUT);
}

if ($use_deadlock) {
    $relname = "deadcontend";
    $indname = "deadcontendind";
} else {
    $relname = "contend";
    $indname = "contendind";
}
$begin = "begin\\g\n";
$end = "end\\g\n";
$destroy = "destroy $relname\\g\n";
$create = "create $relname (value=int4)\\g\n";
$define = "define index $indname on $relname using btree (value int4_ops)\\g\n";

$which_mon = $mon_fh[0];
print $which_mon, $destroy;
print $which_mon $destroy;
print $which_mon, $create;
print $which_mon $create;
print $which_mon, $define;
print $which_mon $define;
# to make sure it's done
sleep 20;

for ($i = 0; $i < $max_iters; ++$i) {
    $qtype = int(rand() * 3);
    $which_mon = $mon_fh[int(rand() * ($n_mon + 1))];
    $value = int(rand() * 5000);

    $append = "append $relname (value = $value)\\g\n";
    $retrieve = "retrieve ($relname.all) where $relname.value < $value\\g\n";
    $replace = "replace $relname (value = $value + 1) where $relname.value = $value\\g\n";

    if ($qtype == 0) {
	print $which_mon, $append;
	print $which_mon $append;
    } elsif ($qtype == 1) {
	print $which_mon, $retrieve;
	print $which_mon $retrieve;
    } else {
	if ($use_deadlock == 1) {
	    print $which_mon, $begin;
	    print $which_mon $begin;
	    print $which_mon, $retrieve;
	    print $which_mon $retrieve;
	}
	print $which_mon, $replace;
	print $which_mon $replace;
	if ($use_deadlock == 1) {
	    print $which_mon, $end;
	    print $which_mon $end;
	}
    }
}

for ($i = 0; $i <= $n_mon; ++$i) {
    # wait for the processes to complete
    close($mon_fh[$i]);
}

exit(0);

#
# Usage
#
# exit with a helpful usage message.
#
sub Usage {
    print "Usage: $program [-d database] [-m monitors] [-n iterations]\n";
    print "\tyou may also specify one of:\n";
    print "\t-D\ttest deadlock handling\n";
    exit(1);
}
