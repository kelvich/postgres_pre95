// This is an include file for -*- C++ -*-
//
// Minimal library for gluing the POSTGRES LIBPQ cruft to C++,
// courtesy of Justin Smith (jsmith@mcs.drexel.edu), 17 Mar 1993
//
// Hacked to use stock POSTGRES headers - pma, 09 June 1993
//
// $Header$
//

#ifndef _pgdb_h
#define _pgdb_h

#include <iostream.h>
#include <string.h>

// anything included here that #include's C++ headers *may* have 
// problems with this...
extern "C" {
#define PROTOTYPES
#undef offsetof			// we define this in tmp/c.h ...
#include "tmp/libpq-fe.h"

    // These are for applications.
    extern char* PQhost;	// machine on which the backend is running
    extern char* PQport;	// comm. port with the postgres backend.
}

class pgdb {
    PortalBuffer*	portalbuf;
    char		res[PortalNameLength+2];// query result string
    char		dbname[NameLength+1];
  
public:
    pgdb() { cout << "You must have a database name" << endl; }
    
    pgdb(char* name) {
	(void) strncpy(dbname, name, NameLength);
	PQsetdb(name);
    }
    
    ~pgdb() { PQfinish(); }
    
    // Sends a query (as a char string) to the database.
    // Ex: dbname << "retrieve(pg_user.all)";
    void operator<<(char*);
    
    // Gets the number of groups. 
    int ngroups();

    // Gets the number of fields for a given group (number) 
    int nfields(int);

    // Gets the number of tuples in a given group.
    int ntuples(int);

    // Gets the total number of tuples.
    int ntuples();
    
    // Gets the number of fields for records of group 0.
    int nfields() {
	return nfields(0);
    }
    
    // Gets the name of field par1 in group par2
    const char* fieldname(int, int);
    
    // Gets the name of field par1 in group 0.
    const char* fieldname(int);
    
    // Retrieves the data in the named field from the numbered tuple
    // as a character string.
    char* fielddata(int, char*);

    // Retrieves the length of data in the named field from the numbered
    // tuple.
    int fieldlength(int, char*);
};

void pgdb::operator<<(char* query)
{
    char* tmpres = PQexec(query);	// pointer to static buffer
    (void) strncpy(res, tmpres, PortalNameLength+1);
    portalbuf = (PortalBuffer*) NULL;
    if (*res == 'E') {
	cout << "error while executing \"" << query << "\"" << endl;
	libpq_raise(&ProtocolError, "pgdb::operator: backend died.\n");
    }
    if (*res == 'P') {
	portalbuf = PQparray(res + 1);
    }
}

int pgdb::ngroups()
{
    int outp = 0;
    /* Tuples in the portal appear in GROUPS */
    if (portalbuf) {
	outp = PQngroups(portalbuf);
    } else {
	cout << "Attempted ngroups with no retrieve" << endl;
    }
    return(outp);
}

int pgdb::nfields(int groupno)
{
    int outp = 0;
    if (portalbuf) {
	outp = PQnfieldsGroup(portalbuf,groupno);
    } else {
	cout << "Attempted nfields with no retrieve" << endl;
    }
    return(outp);
}

const char* pgdb::fieldname(int groupno, int n)
{
    char* outp = (char*) NULL;
    if (portalbuf) {
	outp = PQfnameGroup(portalbuf,groupno,n);
    } else {
	cout << "Attempted fieldname with no retrieve" << endl;
    }
    return(outp);
}

const char* pgdb::fieldname(int n)
{
    return(fieldname(0,n));
}

char* pgdb::fielddata(int tupleno, char* fieldname)
{
    int fnumber, groupno;
    char* outp = (char*) NULL;
    if (portalbuf) {
	groupno = PQgetgroup(portalbuf, tupleno);
	fnumber = PQfnumberGroup(portalbuf, groupno, fieldname);
	outp = PQgetvalue(portalbuf, tupleno, fnumber);
    } else {
	cout << "Attempted fielddata with no retrieve" << endl;
    }
    return(outp);
}

int pgdb::fieldlength(int tupleno, char* fieldname)
{
    int fnumber, groupno;
    int outp = 0;
    if (portalbuf) {
	groupno = PQgetgroup(portalbuf, tupleno);
	fnumber = PQfnumberGroup(portalbuf, groupno, fieldname);
	outp = PQgetlength(portalbuf, tupleno, fnumber);
    } else {
	cout << "Attempted fieldlength with no retrieve" << endl;
    }
    return(outp);
}

int pgdb::ntuples(int groupno)
{
    int outp = 0;
    if (portalbuf) {
	outp = PQntuplesGroup(portalbuf, groupno);
    } else {
	cout << "Attempted ntuples with no retrieve" << endl;
    }
    return(outp);
}

int pgdb::ntuples()
{
    int outp = 0;
    if (portalbuf) {
	outp = PQntuples(portalbuf);
    } else {
	cout << "Attempted ntuples with no retrieve" << endl;
    }
    return(outp);
}

#endif /* _pgdb_h */
