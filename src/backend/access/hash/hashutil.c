/*
 *  btutils.c -- Utility code for Postgres btree implementation.
 */

#include "tmp/c.h"
#include "tmp/postgres.h"

#include "storage/bufmgr.h"
#include "storage/bufpage.h"
#include "storage/page.h"

#include "fmgr.h"
#include "utils/log.h"
#include "utils/rel.h"
#include "utils/excid.h"

#include "access/heapam.h"
#include "access/genam.h"
#include "access/iqual.h"
#include "access/ftup.h"
#include "access/hash.h"

RcsId("$Header$");

ScanKey
_hash_mkscankey(rel, itup, metap)
    Relation rel;
    IndexTuple itup;
    HashMetaPage metap;
{
    ScanKey skey;
    TupleDescriptor itupdesc;
    int natts;
    int i;
    Datum arg;
    RegProcedure proc;
    Boolean null;

    natts = rel->rd_rel->relnatts;
    itupdesc = RelationGetTupleDescriptor(rel);

    skey = (ScanKey) palloc(natts * sizeof(ScanKeyEntryData));

    for (i = 0; i < natts; i++) {
	arg = (Datum) index_getattr(itup, i + 1, itupdesc, &null);
	proc = metap->hashm_procid;
	ScanKeyEntryInitialize((ScanKeyEntry) &(skey->data[i]),
						   0x0, (AttributeNumber) (i + 1), proc, arg);
    }

    return (skey);
}	

void
_hash_freeskey(skey)
    ScanKey skey;
{
    pfree (skey);
}


bool
_hash_checkqual(scan, itup)
    IndexScanDesc scan;
    IndexTuple itup;
{
    if (scan->numberOfKeys > 0)
	return (ikeytest(itup, scan->relation,
			 scan->numberOfKeys, &(scan->keyData)));
    else
	return (true);
}

HashItem 
_hash_formitem(itup)
    IndexTuple itup;
{
    int nbytes_hitem;
    HashItem hitem;
    Size tuplen;
    extern OID newoid();

    /* disallow nulls in hash keys */
    if (itup->t_info & INDEX_NULL_MASK)
	elog(WARN, "hash indices cannot include null keys");

    /* make a copy of the index tuple with room for the sequence number */
    tuplen = IndexTupleSize(itup);
    nbytes_hitem = tuplen +
	(sizeof(HashItemData) - sizeof(IndexTupleData));

    hitem = (HashItem) palloc(nbytes_hitem);
    bcopy((char *) itup, (char *) &(hitem->hash_itup), tuplen);

    hitem->hash_oid = newoid();
    return (hitem);
}

Bucket
_hash_call(rel, metad, key)
        Relation rel;
	HashMetaPageData *metad;
	Datum key;
{
	uint32 n;
	Bucket bucket;
	RegProcedure proc;

	proc = metad->hashm_procid;
	n = (uint32) fmgr(proc, key);
	bucket = n & metad->hashm_highmask;
	if (bucket > metad->hashm_maxbucket)
		bucket = bucket & metad->hashm_lowmask;
	return (bucket);
}

#include <sys/types.h>

uint32
_hash_log2(num)
	u_int num;
{
	uint32 i, limit;

	limit = 1;
	for (i = 0; limit < num; limit = limit << 1, i++);
	return (i);
}







