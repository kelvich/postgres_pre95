/* ----------------------------------------------------------------
 *   FILE
 *	miscinit.c
 *	
 *   DESCRIPTION
 *	miscellanious initialization support stuff
 *
 *   INTERFACE ROUTINES
 *
 *   NOTES
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#include "tmp/postgres.h"

#include "tmp/hasht.h"		/* for EnableHashTable, etc. */
#include "tmp/portal.h"		/* for EnablePortalManager, etc. */
#include "utils/exc.h"		/* for EnableExceptionHandling, etc. */
#include "utils/mcxt.h"		/* for EnableMemoryContext, etc. */
#include "utils/log.h"

#include "tmp/miscadmin.h"

/* ----------------------------------------------------------------
 *	processing mode support stuff (used to be in pmod.c)
 * ----------------------------------------------------------------
 */
static ProcessingMode	Mode = NoProcessing;

bool
IsNoProcessingMode()
{
    return ((bool)(Mode == NoProcessing));
}

bool
IsBootstrapProcessingMode()
{
    return ((bool)(Mode == BootstrapProcessing));
}

bool
IsInitProcessingMode()
{
    return ((bool)(Mode == InitProcessing));
}

bool
IsNormalProcessingMode()
{
    return ((bool)(Mode == NormalProcessing));
}

void
SetProcessingMode(mode)
    ProcessingMode	mode;
{
    AssertArg(mode == NoProcessing || mode == BootstrapProcessing || mode == InitProcessing || mode == NormalProcessing);

    Mode = mode;
}

/* ----------------------------------------------------------------
 *	ReinitAtFirstTransaction()
 *	InitAtFirstTransaction()
 *
 *	This is obviously some half-finished hirohama-ism that does
 *	nothing and should be removed.
 * ----------------------------------------------------------------
 */
void
ReinitAtFirstTransaction()
{
    elog(FATAL, "ReinitAtFirstTransaction: not implemented, yet");
}

void
InitAtFirstTransaction()
{
    if (TransactionInitWasProcessed) {
	ReinitAtFirstTransaction();
    }
    
    /* Walk the relcache? */
    TransactionInitWasProcessed = true;	/* XXX ...InProgress also? */
}

/* ----------------------------------------------------------------
 *		database path / name support stuff
 * ----------------------------------------------------------------
 */
static String	DatabasePath = NULL;
static String	DatabaseName = NULL;

String
GetDatabasePath()
{
    return DatabasePath;
}

String
GetDatabaseName()
{
    return DatabaseName;
}

void
SetDatabasePath(path)
    String path;
{
    DatabasePath = path;
}

void
SetDatabaseName(name)
    String name;
{
    extern NameData MyDatabaseNameData;
    
    /* ----------------
     *	save the database name in MyDatabaseNameData.
     *  XXX we currently have MyDatabaseName, MyDatabaseNameData and
     *  DatabaseName.  What uses each of these?? this duality should
     *  be eliminated! -cim 10/5/90
     * ----------------
     */
    strncpy(&MyDatabaseNameData, name, 16);
    
    DatabaseName = (String) &MyDatabaseNameData;
}

/* ----------------------------------------------------------------
 *	GetUserId and SetUserId support (used to be in pusr.c)
 * ----------------------------------------------------------------
 */
static ObjectId	UserId = InvalidObjectId;

/* ----------------
 *	GetUserId
 * ----------------
 */
ObjectId
GetUserId()
{
    AssertState(ObjectIdIsValid(UserId));
    return (UserId);
}

/* ----------------
 *	SetUserId
 *
 *  XXX perform lookup in USER relation
 * ----------------
 */
void
SetUserId()
{
    UserId = getuid();
}
