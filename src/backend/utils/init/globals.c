
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
/* #include <signal.h> */
#include "catname.h"
#include "heapam.h"
#include "lmgr.h"
#include "log.h"
#include "master.h"
#include "postgres.h"
#include "rproc.h"
#include "sinval.h"
#include "sinvaladt.h"
#include "tqual.h"
#include <math.h>

RcsId("$Header$");

int Debugfile, Ttyfile, Dblog, Slog;
int Portfd, Packfd, Slog, Pipefd;

BackendId	MyBackendId;
BackendTag	MyBackendTag;
NameData	MyDatabaseNameData;
Name		MyDatabaseName = &MyDatabaseNameData;
bool		MyDatabaseIdIsInitialized = false;
ObjectId	MyDatabaseId = InvalidObjectId;
bool		TransactionInitWasProcessed = false;

struct	bcommon	Ident;	/* moved to dlog */

