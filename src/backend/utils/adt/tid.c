/* ----------------------------------------------------------------
 *   FILE
 * 	tid.c
 *
 *   DESCRIPTION
 *	Functions for the built-in type tuple id
 *
 *   NOTES
 *	input routine largely stolen from boxin().
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#include "tmp/postgres.h"

RcsId("$Header$");

#include "storage/block.h"
#include "storage/off.h"
#include "storage/page.h"
#include "storage/part.h"
#include "storage/itemptr.h"

#include "utils/palloc.h"

#define LDELIM		'('
#define RDELIM		')'
#define	DELIM		','
#define NTIDARGS	3

/* ----------------------------------------------------------------
 *	tidin
 * ----------------------------------------------------------------
 */
ItemPointer
tidin(str)
    char *str;
{
    char		*p, *coord[NTIDARGS];
    int			i;
    ItemPointer		result;
    
    BlockNumber  	blockNumber;
    PageNumber   	pageNumber;
    OffsetNumber	offsetNumber;
    PositionId	 	positionId;
    
    if (str == NULL)
	return NULL;

    for (i = 0, p = str; *p && i < NTIDARGS && *p != RDELIM; p++)
	if (*p == DELIM || (*p == LDELIM && !i))
	    coord[i++] = p + 1;
    
    if (i < NTIDARGS - 1)
	return NULL;
    
    blockNumber =	(BlockNumber) 	atoi(coord[0]);
    pageNumber = 	(PageNumber)	atoi(coord[1]);
    offsetNumber = 	(OffsetNumber)	atoi(coord[2]);
    
    result = (ItemPointer) palloc(sizeof(ItemPointerData));

    ItemPointerSet(result, SinglePagePartition,
		   blockNumber, pageNumber, offsetNumber);

    return result;
}

/* ----------------------------------------------------------------
 *	tidout
 * ----------------------------------------------------------------
 */
char *
tidout(itemPtr)
    ItemPointer itemPtr;
{
    BlockNumber  	blockNumber;
    PageNumber   	pageNumber;
    OffsetNumber	offsetNumber;
    PositionId	 	positionId;
    BlockId	 	blockId;
    PagePartition	partition;
    char		buf[32];
    char		*str;
    
    blockId =	 &itemPtr->blockData;
    positionId = &itemPtr->positionData;
    partition =  SinglePagePartition;

    blockNumber =  BlockIdGetBlockNumber(blockId);
    pageNumber =   PositionIdGetPageNumber(positionId, partition);
    offsetNumber = PositionIdGetOffsetNumber(positionId, partition);
    
    sprintf(buf, "(%d,%d,%d)", blockNumber, pageNumber, offsetNumber);

    str = (char *) palloc(strlen(buf)+1);
    strcpy(str, buf);

    return str;
}
