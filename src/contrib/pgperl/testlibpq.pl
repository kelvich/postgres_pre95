#!./pgperl
#
# $Header$
#
# An example of how to use Postgres from perl.
# This example is modelled after the example in the libpq reference manual.

# Be certain you execute this from the contrib/pgperl directory of your
# Postgres distribution, and that the postgres ``bin'' directory
# is in your path.  Also, you *must* have the postmaster running!

$dbname = "pgperltest";
$pgdata = $ENV{'PGDATA'};

&init_handler();

# Destroy then create the database
# an error is ok in destroydb, since the database may not exist.
printf("Destroying database $dbname\n");
system("destroydb $dbname");
printf("Creating database $dbname\n");
if (system("createdb $dbname") / 256) {
    die("$0: createdb failed on $pgdata/$dbname\n");
}

# specify the database to access
&PQsetdb ($dbname);
$thedb = &PQdb();
printf("Accessing database $thedb\n");

printf("\nCreating relation person:\n");
&test_create();

printf("\nRelation person before appends:\n");
&test_functions();

printf("\nAppending to relation person:\n");
&test_append();

printf("\nRelation person after appends:\n");
&test_functions();

printf("\nTesting copy:\n");
&test_copy();

printf("\nRelation person after copy:\n");
&test_functions();

printf("\nTesting other things:\n");
&test_rest();

printf("\nRemoving from relation person:\n");
&test_remove();

printf("\nRelation person after removes:\n");
&test_functions();

printf("\nPrinting values of global variables:\n");
&test_vars();

printf("\nTests complete!\n");
# finish execution
&PQfinish ();

exit(0);

sub doPQexec {
    local($query) = @_;
    local($result);

    $result = &PQexec($query);
    if ($result eq "R" || $result eq "E") {
	die("$0: doPQexec: the query\n\t$query\nproduced the error\n\t$PQerrormsg");
    }
}

sub test_create {
    local($query, $result);

    $query = "create person (name = char16, age = int4, location = point)";
    printf("query = %s\n", $query);
    &doPQexec($query);
}

sub test_append {
    local($i, $query);

    &doPQexec("begin"); # transaction
    for ($i=50; $i <= 150; $i = $i + 10) {
	$query = "append person (name = \"fred\", age = $i, location = \"($i,10)\"::point)";
	printf("query = %s\n", $query);
	&doPQexec($query);
    }
    &doPQexec("end"); # transaction
}

sub test_remove {
    local($i, $query);
    for ($i=50; $i <= 150; $i = $i + 10) {
	$query = "delete person where person.age = $i ";
	printf("query = %s\n", $query);
	&doPQexec($query);
    }
}

sub test_functions {
    local($p, $g, $t, $n, $m, $k, $i, $j);

    # fetch tuples from the person table
    &doPQexec ("begin");
    &doPQexec ("retrieve portal eportal (person.all)");
    &doPQexec ("fetch all in eportal");
    
    # examine all the tuples fetched
    $p = &PQparray ("eportal");	# remember: $p is a pointer !
    $g = &PQngroups ($p);
    $t = 0;

    for ($k=0; $k < $g; $k++) {
	printf("New tuple group:\n");
	$n = &PQntuplesGroup($p, $k);
	$m = &PQnfieldsGroup($p, $k);
	
	# print out the attribute names
	for ($i=0; $i < $m; $i++) {
	    printf("%-15s", &PQfnameGroup($p, $k, $i));
	}
	printf("\n");
	
	# print out the tuples
	for ($i=0; $i < $n; $i++) {
	    for ($j=0; $j < $m; $j++) {
		printf("%-15s", &PQgetvalue($p, $t + $i, $j));
	    }
	    printf("\n");
	}
	$t = $t + $n;
    }

    # close the portal
    &doPQexec ("close eportal");
    &doPQexec ("end");
    &PQclear("eportal");
}

sub test_vars {
    printf("PQhost = \"%s\"\n",		$PQhost);
    printf("PQport = \"%s\"\n",		$PQport);
    printf("PQtty = \"%s\"\n", 		$PQtty);
    printf("PQoption = \"%s\"\n",  	$PQoption);
    printf("PQdatabase = \"%s\"\n", 	$PQdatabase);
    printf("PQportset = %d\n", 		$PQportset);
    printf("PQxactid = %d\n",  		$PQxactid);
    printf("PQtracep = %d\n",  		$PQtracep);
    printf("PQerrormsg = \"%s\"\n",	$PQerrormsg);
}

sub test_copy {
    &doPQexec("copy person from stdin");
    &PQputline("bill	21	(1,2)\n");
    &PQputline("bob	61	(3,4)\n");
    &PQputline("sally	39	(5,6)\n");
    &PQputline(".\n");
    &PQendcopy();
}

sub test_rest {
    printf("Opening 2 portals:\n");
    &doPQexec ("begin");
    &doPQexec ("retrieve portal eportal (person.all)");
    &doPQexec ("fetch all in eportal");
    &doPQexec ("retrieve portal fportal (person.all)");
    &doPQexec ("fetch all in fportal");
    printf("Number of portals open: %d\n", &PQnportals(0));
    @names = &PQpnames (0);
    print "Portal names: ", join(', ',@names), ".\n";
    $p = &PQparray ("eportal");
    printf("Portal eportal %s asynchronous.\n",&PQrulep($p) ? "is" : "is not");
    printf("Portal eportal has %d tuples.\n",&PQntuples($p));
    printf("Portal eportal has %d instances.\n",&PQninstances($p));
    printf("Portal eportal has %d groups.\n",&PQngroups($p));
    printf("Portal eportal group 0 has %d instances.\n",&PQninstancesGroup($p,0));
    printf("Portal eportal tuple 0 has %d fields.\n",&PQnfields($p, 0));
    printf("Portal eportal tuple 0 field 2 is %d bytes long.\n",&PQgetlength($p, 0, 2));
    printf("Portal eportal tuple 0 field 2 is type %d.\n",&PQftype($p, 0, 2));
    printf("Portal eportal tuple 0 is in group %d.\n",&PQgetgroup($p, 0));
    printf("Portal eportal tuple 0 field \"location\" is index %d.\n",&PQfnumber($p, 0, "location"));
    printf("Portal eportal tuple 0 field 1 is name \"%s\".\n",&PQfname($p, 0, 1));
    printf("Portal eportal tuples 0 and 1 %s the same type.\n",&PQsametype($p, 0, 1) ? "are" : "are not");
    printf("Portal eportal group 0 field \"location\" is index %d.\n",&PQfnumberGroup($p, 0, "location"));
    printf("Closing 2 portals:\n");
    &doPQexec ("close eportal");
    &doPQexec ("close fportal");
    &doPQexec ("end");
    &PQclear("eportal");
    &PQclear("fportal");
    printf("Number of portals open: %d\n", &PQnportals(0));
    @names = &PQpnames (0);
    print "Portal names: ", join(', ',@names), ".\n";
}

sub init_handler {
    $SIG{'HUP'} = 'handler';
    $SIG{'INT'} = 'handler';
    $SIG{'QUIT'} = 'handler';
}

sub handler {  # 1st argument is signal name
    local($sig) = @_;
    print "Caught a SIG$sig--shutting down connection to Postgres.\n";
    &PQfinish();
    exit(0);
}
