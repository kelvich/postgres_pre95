#!./pgperl
# An example of how to use Postgres 2.0 from perl.
# This example is modelled after the example in the libpq reference manual.
# $Id$
# $Log$
# Revision 1.3  1992/02/15 20:33:20  mer
# updated by George Hartzell of the Stanford Genome project
#
#% Revision 1.2  92/02/07  16:33:47  hartzell
#% *** empty log message ***
#% 
#% Revision 1.1  92/01/29  15:10:42  hartzell
#% Initial revision
#% 
#% Revision 1.2  91/03/08  13:22:41  kemnitz
#% added RCS header.
#% 
#
# $Header: /genome/src/postgres/src/contrib/pgperl/RCS/testlibpq.pl,v 1.2 92/0
2/07 16:33:47 hartzell Exp $
#
#% Revision 1.1  90/10/24  20:31:22  cimarron
#% Initial revision
#% 

&init_handler();

# specify the database to access
&PQsetdb ("cimarron");

printf("creating relation person:\n\n");
&test_create();

printf("Relation person before appends:\n\n");
&test_functions();
&test_append();

printf("Relation person after appends:\n\n");
&test_functions();
&PQreset (); # why do I have to reset the line ?? :-(
&test_remove();

printf("Relation person after removes:\n\n");
&test_functions();
&test_vars();

# finish execution
&PQfinish ();

exit(0);

sub test_create {
    local($query);

    $query = "create person (name = char16, age = int4, location = point)";
    printf("query = %s\n", $query);
    &PQexec($query);
}

sub test_append {
    local($i, $query);

    &PQexec("begin"); # transaction
    for ($i=50; $i <= 150; $i = $i + 10) {
	$query = "append person (name = \"fred\", age = $i, location = \"($i,10
)\"::point)";
	printf("query = %s\n", $query);
	&PQexec($query);
    }
    &PQexec("end"); # transaction
}

sub test_remove {
    local($i, $query);
    for ($i=50; $i <= 150; $i = $i + 10) {
	$query = "delete person where person.age = $i ";
	printf("query = %s\n", $query);
	&PQexec($query);
    }
}

sub test_functions {
    local($p, $g, $t, $n, $m, $k, $i, $j);

    # fetch tuples from the person table
    &PQexec ("begin");
    &PQexec ("retrieve portal eportal (person.all)");
    &PQexec ("fetch all in eportal");
    
    # examine all the tuples fetched
    $p = &PQparray ("eportal");	# remember: $p is a pointer !
    $g = &PQngroups ($p);
    $t = 0;

    for ($k=0; $k < $g; $k++) {
	printf("\nNew tuple group:\n");
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
    &PQexec ("close eportal");
    &PQexec ("end");

    # try some other functions
    printf("\nNumber of portals open: %d\n", &PQnportals(0));
}

sub test_vars {
    printf("PQhost = %s\n", 	$PQhost);
    printf("PQport = %s\n",  	$PQport);
    printf("PQtty = %s\n",  	$PQtty);
    printf("PQoption = %s\n",  	$PQoption);
    printf("PQdatabase = %s\n", $PQdatabase);
    printf("PQportset = %d\n", 	$PQportset);
    printf("PQxactid = %d\n",  	$PQxactid);
    printf("PQinitstr = %s\n", 	$PQinitstr);
    printf("PQtracep = %d\n",  	$PQtracep);
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
