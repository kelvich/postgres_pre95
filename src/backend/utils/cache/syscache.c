/* ----------------------------------------------------------------
 *   FILE
 *	syscache.c
 *	
 *   DESCRIPTION
 *	System cache management routines
 *
 *   INTERFACE ROUTINES
 *	SearchSysCacheTuple()
 *	SearchSysCacheStruct()
 *	SearchSysCacheGetAttribute()
 *	TypeDefaultRetrieve()
 *
 *   NOTES
 *	These routines allow the parser/planner/executor to perform
 *	rapid lookups on the contents of the system catalogs.  (Well,
 *	mabey not so rapid just yet..)
 *
 *	see lib/catalog/syscache.h for a list of the cache id's
 *
 * old notes:
 *	LISP code should call SearchSysCacheStruct with preallocated space
 *	 (or SearchSysCacheGetAttribute for variable-length or system fields).
 *	C code should call SearchSysCacheTuple -- it's slightly more efficient.
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */
#include "tmp/c.h"

RcsId("$Header$");
 
#include "access/heapam.h"
#include "access/htup.h"
#include "catalog/catname.h"
#include "utils/catcache.h"
#include "utils/log.h"
#include "utils/palloc.h"
#include "nodes/pg_lisp.h"
 
/* ----------------
 *	hardwired attribute information comes from system catalog files.
 * ----------------
 */
#include "catalog/pg_am.h"
#include "catalog/pg_amop.h"
#include "catalog/pg_attribute.h"
#include "catalog/pg_index.h"
#include "catalog/pg_inherits.h"
#include "catalog/pg_language.h"
#include "catalog/pg_opclass.h"
#include "catalog/pg_operator.h"
#include "catalog/pg_proc.h"
#include "catalog/pg_prs2rule.h"
#include "catalog/pg_prs2plans.h"
#include "catalog/pg_prs2stub.h"
#include "catalog/pg_relation.h"
#include "catalog/pg_type.h"
#include "catalog/pg_rewrite.h"
#include "catalog/pg_aggregate.h"
#include "catalog/pg_naming.h"
#include "utils/large_object.h"
#include "catalog/pg_lobj.h"
#include "catalog/pg_listener.h"
 
extern bool	AMI_OVERRIDE;	/* XXX style */
 
#define BIGENDIAN		/* e.g.: 68000, MIPS, Tahoe */
/*#define LITTLEENDIAN		/* e.g.: VAX, i386 */

#include "catalog/syscache.h"
    
/* ----------------
 *	Warning:  cacheinfo[] below is changed, then be sure and
 *	update the magic constants in syscache.h!
 *
 *	Note: currently all the catalog cache index names are
 *	NULL.  This will change soon -cim 1/13/91.
 * ----------------
 */
static struct cachedesc cacheinfo[] = {
    { &AccessMethodOperatorRelationName,	/* AMOPOPID */
	  2,
	  { AccessMethodOperatorOperatorClassIdAttributeNumber,
		AccessMethodOperatorOperatorIdAttributeNumber,
		0,
		0 },
	  sizeof(struct amop),
      NULL },
    { &AccessMethodOperatorRelationName,	/* AMOPSTRATEGY */
	  3,
	  { AccessMethodOperatorAccessMethodIdAttributeNumber,
		AccessMethodOperatorOperatorClassIdAttributeNumber,
		AccessMethodOperatorStrategyAttributeNumber,
		0 },
	  sizeof(struct amop),
      NULL },
    { &AttributeRelationName,			/* ATTNAME */
	  2,
	  { AttributeRelationIdAttributeNumber,
		AttributeNameAttributeNumber,
		0,
		0 },
	  sizeof(struct attribute),
      NULL },
    { &AttributeRelationName,			/* ATTNUM */
	  2,
	  { AttributeRelationIdAttributeNumber,
		AttributeNumberAttributeNumber,
		0,
		0 },
	  sizeof(struct attribute),
      NULL },
    { &IndexRelationName,			/* INDEXRELID */
	  1,
	  { IndexRelationIdAttributeNumber,
		0,
		0,
		0 },
	  sizeof(struct index),
      NULL },
    { &LanguageRelationName,			/* LANNAME */
	  1,
	  { LanguageNameAttributeNumber,
		0,
		0,
		0 },
	  sizeof(struct language) - sizeof(struct varlena),
      NULL },
    { &OperatorRelationName,			/* OPRNAME */
	  4,
	  { OperatorNameAttributeNumber,
		OperatorLeftAttributeNumber,
		OperatorRightAttributeNumber,
		OperatorKindAttributeNumber },
	  sizeof(struct operator),
      NULL },
    { &OperatorRelationName,			/* OPROID */
	  1,
	  { ObjectIdAttributeNumber,
		0,
		0,
		0 },
	  sizeof(struct operator),
      NULL },
    { &ProcedureRelationName,			/* PRONAME */
	  1,
	  { ProcedureNameAttributeNumber,
		0,
		0,
		0 },
	  sizeof(struct proc),
      NULL },
    { &ProcedureRelationName,			/* PROOID */
	  1,
	  { ObjectIdAttributeNumber,
		0,
		0,
		0 },
	  sizeof(struct proc),
      NULL },
    { &RelationRelationName,			/* RELNAME */
	  1,
	  { RelationNameAttributeNumber,
		0,
		0,
		0 },
	  sizeof(struct relation),
      NULL },
    { &RelationRelationName,			/* RELOID */
	  1,
	  { ObjectIdAttributeNumber,
		0,
		0,
		0 },
	  sizeof(struct relation),
      NULL },
    { &TypeRelationName,			/* TYPNAME */
	  1,
	  { TypeNameAttributeNumber,
		0,
		0,
		0 },
	  sizeof(TypeTupleFormData) - sizeof(struct varlena),
      NULL },
    { &TypeRelationName,			/* TYPOID */
	  1,
	  { ObjectIdAttributeNumber,
		0,
		0,
		0},
	  sizeof(TypeTupleFormData) - sizeof(struct varlena),
      NULL },
    { &AccessMethodRelationName,		/* AMNAME */
	  1,
	  { AccessMethodNameAttributeNumber,
		0,
		0,
		0},
	  sizeof(IndexTupleFormData),
      NULL },
    { &OperatorClassRelationName,		/* CLANAME */
	  1,
	  { OperatorClassNameAttributeNumber,
		0,
		0,
		0},
	  sizeof(IndexTupleFormData),
      NULL },
    { &IndexRelationName,			/* INDRELIDKEY */
	  2,
	  { IndexHeapRelationIdAttributeNumber,
		IndexKeyAttributeNumber,
		0,
		0},
	  sizeof(IndexTupleFormData),
      NULL },
    { &InheritsRelationName,			/* INHRELID */
	  2,
	  { InheritsRelationIdAttributeNumber,
		InheritsSequenceNumberAttributeNumber,
		0,
		0},
	  sizeof(InheritsTupleFormD),
      NULL },
    { &Prs2PlansRelationName,			/* PRS2PLANCODE */
	  2,
	  { Prs2PlansRuleIdAttributeNumber,
		Prs2PlansPlanNumberAttributeNumber,
		0,
		0 },
	  sizeof(struct prs2plans) - sizeof(struct varlena),
      NULL },
    { &RewriteRelationName,			/* RULOID */
	  1,
	  { ObjectIdAttributeNumber,
		0,
		0,
		0 },
	  sizeof(FormData_pg_rewrite),
      NULL },
    { &Prs2StubRelationName,			/* PRS2STUB */
	  2,
	  { Anum_pg_prs2stub_prs2relid,
	    Anum_pg_prs2stub_prs2no,
		0,
		0 },
	  sizeof(FormData_pg_prs2stub) - sizeof(struct varlena),
      NULL },
    { &Prs2RuleRelationName,			/* PRS2RULEID	*/
	  1,
	  { Anum_pg_prs2rule_prs2name,
		0,
		0,
		0 },
	  sizeof(FormData_pg_prs2rule),
      NULL },
    { &Prs2RuleRelationName,			/* PRS2EVENTREL	*/
	  1,
	  { ObjectIdAttributeNumber,
		0,
		0,
		0 },
	  sizeof(FormData_pg_prs2rule),
      NULL },
    { &AggregateRelationName,			/*AGGNAME*/
	  1,
	  { AggregateNameAttributeNumber,
	        0,
		0,
		0 },
	   sizeof(FormData_pg_aggregate),
       NULL },    
    { &NamingRelationName,                      /*NAMEREL */
        2,
        { Anum_pg_naming_parent_oid,/*NamingParentOIDAttributeNumber,*/
            Anum_pg_naming_filename,/*NamingFilenameAttributeNumber,*/
            0,
            0 },
        sizeof(FormData_pg_naming),
        NULL },
    { &LargeObjectAssocRelationName,           /*LOBJREL */
        1,
        { Anum_pg_large_object_oid,/*LargeObjectOIDAttributeNumber,*/
            0,
            0,
            0 },
        sizeof (FormData_pg_large_object),
        NULL },
    { &ListenerRelationName,                  /* LISTENREL */
	2,
	{  Anum_pg_listener_relname,
	     Anum_pg_listener_pid,
	     0,
	     0 },
	sizeof(FormData_pg_listener),
	NULL }
};
 
static struct catcache	*SysCache[lengthof(cacheinfo)];
static int32		SysCacheSize = lengthof(cacheinfo);
    
    
/*
 *	zerocaches
 *
 *	Make sure the SysCache structure is zero'd.
 */
zerocaches()
{
    bzero((char *) SysCache, SysCacheSize * sizeof(struct catcache *));
}
 
/*
 * Note:
 *	This function was written because the initialized catalog caches
 *	are used to determine which caches may contain tuples which need
 *	to be invalidated in other backends.
 */
void
InitCatalogCache()
{
    int	cacheId;	/* XXX type */
    
    if (!AMI_OVERRIDE) {
	for (cacheId = 0; cacheId < SysCacheSize; cacheId += 1) {
	    
	    Assert(!PointerIsValid((Pointer)SysCache[cacheId]));
	    
	    SysCache[cacheId] =
		InitSysCache((*cacheinfo[cacheId].name)->data, 
			     cacheinfo[cacheId].nkeys,
			     cacheinfo[cacheId].key);
	    if (!PointerIsValid((char *)SysCache[cacheId])) {
		elog(WARN,
		     "InitCatalogCache: Can't init cache %.16s(%d)",
		     (*cacheinfo[cacheId].name)->data,
		     cacheId);
	    }
	    
	    /* XXX the cacheId should be passed in InitSysCache */
	    CatalogCacheSetId(SysCache[cacheId], cacheId);
	}
    }
}

/*
 *	SearchSysCacheTuple
 *
 *	A layer on top of SearchSysCache that does the initialization and
 *	key-setting for you.
 *
 *	Returns the tuple if one is found, NULL if not.
 *
 *	XXX The tuple that is returned is NOT supposed to be pfree'd!
 */

/**** xxref:
 *           getmyrelids
 *           GetAttributeRelationIndexRelationId
 *           SearchSysCacheStruct
 *           SearchSysCacheGetAttribute
 *           TypeDefaultRetrieve
 ****/
HeapTuple
SearchSysCacheTuple(cacheId, key1, key2, key3, key4)
    int		cacheId;		/* cache selection code */
    char	*key1, *key2, *key3, *key4;
{
    register HeapTuple	tp;
    
    if (cacheId < 0 || cacheId >= SysCacheSize) {
	elog(WARN, "SearchSysCacheTuple: Bad cache id %d", cacheId);
	return((HeapTuple) NULL);
    }
    
    if (!AMI_OVERRIDE) {
	Assert(PointerIsValid(SysCache[cacheId]));
    } else {
	if (!PointerIsValid(SysCache[cacheId])) {
	    SysCache[cacheId] =
		InitSysCache((*cacheinfo[cacheId].name)->data, 
			     cacheinfo[cacheId].nkeys,
			     cacheinfo[cacheId].key);
	    if (!PointerIsValid(SysCache[cacheId])) {
		elog(WARN,
		     "InitCatalogCache: Can't init cache %.16s(%d)",
		     (*cacheinfo[cacheId].name)->data,
		     cacheId);
	    }
	    
	    /* XXX the cacheId should be passed in InitSysCache */
	    CatalogCacheSetId(SysCache[cacheId], cacheId);
	}
    }
    
    tp = SearchSysCache(SysCache[cacheId], key1, key2, key3, key4);
    if (!HeapTupleIsValid(tp)) {
#ifdef CACHEDEBUG
	elog(DEBUG,
	     "SearchSysCacheTuple: Search %s(%d) %d %d %d %d failed", 
	     (*cacheinfo[cacheId].name)->data,
	     cacheId, key1, key2, key3, key4);
#endif
	return((HeapTuple) NULL);
    }
    return(tp);
}
 
/*
 *	SearchSysCacheStruct
 *
 *	Fills 's' with the information retrieved by calling SearchSysCache()
 *	with arguments key1...key4.  Retrieves only the portion of the tuple
 *	which is not variable-length.
 *
 *	NOTE: we are assuming that non-variable-length fields in the system
 *	      catalogs will always be defined!
 *
 *	Returns 1L if a tuple was found, 0L if not.
 */
 
int32
SearchSysCacheStruct(cacheId, returnStruct, key1, key2, key3, key4)
    int		cacheId;		/* cache selection code */
    char	*returnStruct;		/* return struct (preallocated!) */
    char	*key1, *key2, *key3, *key4;
{
    HeapTuple	tp;
    
    if (!PointerIsValid(returnStruct)) {
	elog(WARN, "SearchSysCacheStruct: No receiving struct");
	return(0);
    }
    tp = SearchSysCacheTuple(cacheId, key1, key2, key3, key4);
    if (!HeapTupleIsValid(tp))
	return(0);
    bcopy((char *) GETSTRUCT(tp), returnStruct, cacheinfo[cacheId].size);
    return(1);
}
 
/*
 *	Stuff to interface to LISP (mostly in lisplib/lsyscache.l).
 *	If you're building a test program, you don't want this.
 *
 *	XXX These are really ugly.
 */
static int32 heapTupleAttributeLength[] = {
    0,
    sizeof(ItemPointerData),				/* CTID */
    Max(sizeof(ItemPointerData), sizeof(RuleLock)),	/* LOCK */
    sizeof(ObjectId),					/* OID */
    sizeof(XID),sizeof(CID),				/* XMIN,CMIN */
    sizeof(XID),sizeof(CID),				/* XMAX,CMAX */
    sizeof(ItemPointerData),sizeof(ItemPointerData),	/* CHAIN,ANCHOR */
    sizeof(ABSTIME),sizeof(ABSTIME),			/* TMIN,TMAX */
    sizeof(char)					/* VTYPE */
};
static Boolean heapTupleAttributeByValue[] = {
    0,
    0,							/* CTID */
    0,							/* LOCK */
    1, 							/* OID */
    1, 1,	 					/* XMIN,CMIN */
    1, 1,	 					/* XMAX,CMAX */
    0, 0,						/* CHAIN,ANCHOR */
    1, 1,						/* TMIN,TMAX */
    1							/* VTYPE */
};
 
/*
 *	SearchSysCacheGetAttribute
 *
 *	Returns the attribute corresponding to 'attributeNumber' for
 *	a given cached tuple.
 *
 *	XXX This re-opens a relation, so this is slower.
 */
/**** xxref:
 *           TypeDefaultRetrieve
 ****/
LispValue
SearchSysCacheGetAttribute(cacheId, attributeNumber, key1, key2, key3, key4)
    int			cacheId;
    AttributeNumber	attributeNumber;
    char 		*key1, *key2, *key3, *key4;
{
    HeapTuple	tp;
    char	*cacheName;
    Relation	relation;
    int32	attributeLength, attributeByValue;
    Boolean	isNull;
    char	*attributeValue;
    LispValue	returnValue;
    
    tp = SearchSysCacheTuple(cacheId, key1, key2, key3, key4);
    cacheName = (*cacheinfo[cacheId].name)->data;
    
    if (!HeapTupleIsValid(tp)) {
#ifdef	CACHEDEBUG
	elog(DEBUG,
	     "SearchSysCacheGetAttribute: Lookup in %s(%d) failed",
	     cacheName, cacheId);
#endif	/* defined(CACHEDEBUG) */
	return(LispNil);
    }
    
    relation = heap_openr(cacheName);
    
    if (attributeNumber < 0 &&
	attributeNumber > FirstLowInvalidHeapAttributeNumber) {
	attributeLength = heapTupleAttributeLength[-attributeNumber];
	attributeByValue = heapTupleAttributeByValue[-attributeNumber];
    } else if (attributeNumber > 0 &&
	       attributeNumber <= relation->rd_rel->relnatts) {
	attributeLength =
	    relation->rd_att.data[attributeNumber-1]->attlen;
	attributeByValue =
	    relation->rd_att.data[attributeNumber-1]->attbyval;
    } else {
	elog(WARN, 
	     "SearchSysCacheGetAttribute: Bad attr # %d in %s(%d)",
	     attributeNumber, cacheName, cacheId);
	return(LispNil);
    }
    
    attributeValue = heap_getattr(tp,
				  (Buffer) 0,
				  attributeNumber,
				  &relation->rd_att,
				  &isNull);
    
    if (isNull) {
	/* actually this is a FATAL error -hirohama */
	elog(DEBUG,
	     "SearchSysCacheGetAttribute: Null attr #%d in %s(%d)",
	     attributeNumber, cacheName, cacheId);
	return(LispNil);
    }
    
    LISP_GC_OFF;
    
    if (attributeByValue) {
	returnValue = lispInteger((int) attributeValue);
    } else {
	char	*tmp;
	int size = (attributeLength < 0)
	    ? VARSIZE((struct varlena *) attributeValue) /* variable length */
	    : attributeLength;				 /* fixed length */
	
	tmp = (char *) palloc(size);
	bcopy(attributeValue, tmp, size);
	returnValue = (LispValue)tmp;
    }
    
    LISP_GC_PROTECT(returnValue);
    LISP_GC_ON;
    
    heap_close(relation);
    return(returnValue);
}
 
/*
 * DANGER!!!  Bizarre byte-ordering hacks follow.-hirohama
 *				(nest ten miles) -jc
 */
/*
 *	TypeDefaultRetrieve
 *
 *	Given a type OID, return the typdefault field associated with that
 *	type.  The typdefault is returned as the car of a dotted pair which
 *	is passed to TypeDefaultRetrieve by the calling routine.
 *
 *	Returns a fixnum for types which are passed by value and a ppreserve'd
 *	vectori for types which are not.
 */
/**** xxref:
 *           <apparently-unused>
 ****/
LispValue
TypeDefaultRetrieve(typId)
    ObjectId		typId;
{
    HeapTuple		typeTuple;
    TypeTupleForm	type;
    int32		typByVal, typLen;
    LispValue		value;
    struct varlena	*typDefault;
    int32		dataSize;
    LispValue		returnValue;
    
    typeTuple = SearchSysCacheTuple(TYPOID,
				    (char *) typId,
				    (char *) NULL,
				    (char *) NULL,
				    (char *) NULL);
    
    if (!HeapTupleIsValid(typeTuple)) {
#ifdef	CACHEDEBUG
	elog(DEBUG, "TypeDefaultRetrieve: Lookup in %s(%d) failed",
	     (*cacheinfo[TYPOID].name)->data, TYPOID);
#endif	/* defined(CACHEDEBUG) */
	return(LispNil);
    }
    
    type = (TypeTupleForm) GETSTRUCT(typeTuple);
    typByVal = type->typbyval;
    typLen = type->typlen;
    
    value = SearchSysCacheGetAttribute(TYPOID,
				       TypeDefaultAttributeNumber,
				       (char *) typId,
				       (char *) NULL,
				       (char *) NULL,
				       (char *) NULL);
    
    if (value == LispNil) {
#ifdef	CACHEDEBUG
	elog(DEBUG, "TypeDefaultRetrieve: No extractable typdefault",
	     (*cacheinfo[TYPOID].name)->data, TYPOID);
#endif	/* defined(CACHEDEBUG) */
	return(LispNil);
	
    }
    
    typDefault = (struct varlena *) value;
    dataSize = VARSIZE(typDefault) - sizeof(typDefault->vl_len);
    
    /*
     * If the attribute is passed by value, copy the data bytes left to
     * right into the return structure.  Otherwise, copy the entire struct
     * varlena (which works out to be the same as ppreserve()'ing the
     * vl_dat portion).
     *
     * XXX This is garbage.  The varlena should contain the external form,
     *     which should be converted using fmgr() -- it would be easier
     *     to specify and maintain that way.  But nooooo...catalogs have
     *     to contain internal form attributes....
     */
    LISP_GC_OFF;
    
    if (typByVal) {
	int32	aligned = 0;
	
	if (dataSize == typLen) {
	    bcopy(VARDATA(typDefault),
		  /* 
		   * The data itself is assumed to be the correct
		   * byte-order; however, which end we copy the
		   * bytes into does depend on the processor...
		   */
		  
#ifdef LITTLEENDIAN
		  /* flush-left for little-endian (LSB..MSB) */
		  (char *) &aligned,
#else /* !LITTLEENDIAN */
#ifdef BIGENDIAN			      
		  /* flush-right for big-endian (MSB..LSB) */
		  ((char *) &aligned) + (sizeof(aligned) -
					 dataSize),
#else /* !BIGENDIAN */
		  /* you've got one, you figure it out... */
#endif /* !BIGENDIAN */
#endif /* !LITTLEENDIAN */
		  (int) dataSize);
	    
	    returnValue = lispInteger((int) aligned);
	} else {
	    returnValue = LispNil;
	}
    } else {
	if ((typLen < 0 && dataSize < 0) || dataSize != typLen)
	    returnValue = LispNil;
	else {
	    returnValue = lispVectori(VARSIZE(typDefault));
	    bcopy((char *) typDefault,
		  (char *) LISPVALUE_BYTEVECTOR(returnValue),
		  (int) VARSIZE(typDefault));
	}
    }
    
    LISP_GC_PROTECT(returnValue);
    LISP_GC_ON;
    return(returnValue);
}
 
