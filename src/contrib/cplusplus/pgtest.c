// This file is a ".c" but it's really -*- C++ -*-
//
// This is just a trivial little test program.
//
// $Header$
//

#include "pg.h"

main()
{
    pgdb mydb("template1");
    char *query =
	"retrieve (pg_class.all) where pg_class.relname ~ \"pg_\"";
    
    cout << "Sending" << endl
	 << "\t" << query << endl
	 << "to the backend server." << endl;

    mydb << query;

    int ngroups = mydb.ngroups();
    cout << "Retrieving " << ngroups << " tuple group(s)." << endl;
    for (int groupno = 0; groupno < ngroups; ++groupno) {
	int ntuples = mydb.ntuples();
	cout << "Group " << groupno << " has " << ntuples
	     << " tuple(s)." << endl;
	int nfields = mydb.nfields(groupno);
	cout << "Each tuple contains " << nfields
	     << " attribute(s), called:" << endl;
	for (int attno = 0; attno < nfields; ++attno) {
	    cout << " " << mydb.fieldname(groupno, attno);
	}
	cout << endl;
	
	ntuples = mydb.ntuples(groupno);
	for (int tupno = 0; tupno < mydb.ntuples(); ++tupno) {
	    cout << " (" << tupno << ") "
		 << mydb.fielddata(tupno, "relname")
		 << " (strlen="
		 << mydb.fieldlength(tupno, "relname")
		 << "): ";
	    char *am = mydb.fielddata(tupno, "relam");
	    if (!strcmp(am, "0")) {
		cout << "HEAP" << endl;
	    } else {
		cout << "INDEX (type=" << am << ")" << endl;
	    }
	}
    }
}
