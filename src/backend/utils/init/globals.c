
/*
 *	globals.c	- global variable declarations
 *
 *	Globals used all over the place should be declared here and not
 *	in other modules.  This reduces the number of modules included
 *	from cinterface.a during linking.
 */

#include <fcntl.h>
#include <stdio.h>
#include <strings.h>
#include <sys/file.h>
#include <sys/types.h>
#include <math.h>

/* #include <signal.h> */

#include "tmp/postgres.h"

RcsId("$Header$");

#include "access/heapam.h"
#include "access/tqual.h"
#include "storage/sinval.h"
#include "storage/sinvaladt.h"
#include "storage/lmgr.h"
#include "tmp/master.h"
#include "utils/log.h"

#include "catalog/catname.h"

int		Portfd;
int		Noversion = 0;

char		OutputFileName[MAXPGPATH] = "";

BackendId	MyBackendId;
BackendTag	MyBackendTag;
NameData	MyDatabaseNameData;
Name		MyDatabaseName = &MyDatabaseNameData;
bool		MyDatabaseIdIsInitialized = false;
ObjectId	MyDatabaseId = InvalidObjectId;
bool		TransactionInitWasProcessed = false;

bool		IsUnderPostmaster = false;
bool		IsPostmaster = false;

short		DebugLvl = 0;

