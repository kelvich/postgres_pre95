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
 *	rapid lookups on the contents of the system catalogs.
 *
 *	see catalog/syscache.h for a list of the cache id's
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
#include "catalog/pg_group.h"
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
#include "catalog/pg_user.h"
#include "utils/large_object.h"
#include "catalog/pg_lobj.h"
#include "catalog/pg_listener.h"
 
extern bool	AMI_OVERRIDE;	/* XXX style */
 
#define BIGENDIAN		/* e.g.: 68000, MIPS, Tahoe */
/*#define LITTLEENDIAN		/* e.g.: VAX, i386 */

#include "catalog/syscache.h"
#include "catalog/indexing.h"
    
/* ----------------
 *	Warning:  cacheinfo[] below is changed, then be sure and
 *	update the magic constants in syscache.h!
 * ----------------
 */
static struct cachedesc cacheinfo[] = {
    { &AccessMethodOperatorRelationName,	/* AMOPOPID */
	  2,
	  { Anum_pg_amop_amopclaid,
		Anum_pg_amop_amopid,
		0,
		0 },
	  sizeof(FormData_pg_amop),
      NULL,
      NULL  },
    { &AccessMethodOperatorRelationName,	/* AMOPSTRATEGY */
	  3,
	  { Anum_pg_amop_amopid,
		Anum_pg_amop_amopclaid,
		Anum_pg_amop_amopstrategy,
		0 },
	  sizeof(FormData_pg_amop),
      NULL,
      NULL  },
    { &AttributeRelationName,			/* ATTNAME */
	  2,
	  { Anum_pg_attribute_attrelid,
		Anum_pg_attribute_attname,
		0,
		0 },
	  sizeof(FormData_pg_attribute),
      &AttributeNameIndex,
      AttributeNameIndexScan  },
    { &AttributeRelationName,			/* ATTNUM */
	  2,
	  { Anum_pg_attribute_attrelid,
		Anum_pg_attribute_attnum,
		0,
		0 },
	  sizeof(FormData_pg_attribute),
      &AttributeNumIndex,
      AttributeNumIndexScan  },
    { &IndexRelationName,			/* INDEXRELID */
	  1,
	  { Anum_pg_index_indexrelid,
		0,
		0,
		0 },
	  sizeof(FormData_pg_index) - sizeof(text),
      NULL,
      NULL  },
    { &LanguageRelationName,			/* LANNAME */
	  1,
	  { Anum_pg_language_lanname,
		0,
		0,
		0 },
	  sizeof(FormData_pg_language) - sizeof(struct varlena),
      NULL,
      NULL  },
    { &OperatorRelationName,			/* OPRNAME */
	  4,
	  { Anum_pg_operator_oprname,
		Anum_pg_operator_oprleft,
		Anum_pg_operator_oprright,
		Anum_pg_operator_oprkind },
	  sizeof(FormData_pg_operator),
      NULL,
      NULL  },
    { &OperatorRelationName,			/* OPROID */
	  1,
	  { ObjectIdAttributeNumber,
		0,
		0,
		0 },
	  sizeof(FormData_pg_operator),
      NULL,
      NULL  },
    { &ProcedureRelationName,			/* PRONAME */
	  1,
	  { Anum_pg_proc_proname,
		0,
		0,
		0 },
	  sizeof(FormData_pg_proc) - (sizeof(text) + sizeof(bytea)),
      &ProcedureNameIndex,
      ProcedureNameIndexScan  },
    { &ProcedureRelationName,			/* PROOID */
	  1,
	  { ObjectIdAttributeNumber,
		0,
		0,
		0 },
	  sizeof(FormData_pg_proc) - (sizeof(text) + sizeof(bytea)),
      &ProcedureOidIndex,
      ProcedureOidIndexScan  },
    { &RelationRelationName,			/* RELNAME */
	  1,
	  { Anum_pg_relation_relname,
		0,
		0,
		0 },
	  sizeof(FormData_pg_relation) - sizeof(aclitem),
      &ClassNameIndex,
      ClassNameIndexScan  },
    { &RelationRelationName,			/* RELOID */
	  1,
	  { ObjectIdAttributeNumber,
		0,
		0,
		0 },
	  sizeof(FormData_pg_relation) - sizeof(aclitem),
      &ClassOidIndex,
      ClassOidIndexScan  },
    { &TypeRelationName,			/* TYPNAME */
	  1,
	  { Anum_pg_type_typname,
		0,
		0,
		0 },
	  sizeof(FormData_pg_type) - sizeof(struct varlena),
      &TypeNameIndex,
      TypeNameIndexScan  },
    { &TypeRelationName,			/* TYPOID */
	  1,
	  { ObjectIdAttributeNumber,
		0,
		0,
		0},
	  sizeof(FormData_pg_type) - sizeof(struct varlena),
      &TypeOidIndex,
      TypeOidIndexScan  },
    { &AccessMethodRelationName,		/* AMNAME */
	  1,
	  { Anum_pg_am_amname,
		0,
		0,
		0},
	  sizeof(FormData_pg_am),
      NULL,
      NULL  },
    { &OperatorClassRelationName,		/* CLANAME */
	  1,
	  { Anum_pg_opclass_opcname,
		0,
		0,
		0},
	  sizeof(FormData_pg_opclass),
      NULL,
      NULL  },
    { &IndexRelationName,			/* INDRELIDKEY */
	  2,
	  { Anum_pg_index_indrelid,
		Anum_pg_index_indkey,
		0,
		0},
	  sizeof(FormData_pg_index),
      NULL,
      NULL  },
    { &InheritsRelationName,			/* INHRELID */
	  2,
	  { InheritsRelationIdAttributeNumber,
		InheritsSequenceNumberAttributeNumber,
		0,
		0},
	  sizeof(InheritsTupleFormD),
      NULL,
      NULL  },
    { &Prs2PlansRelationName,			/* PRS2PLANCODE */
	  2,
	  { Anum_pg_prs2plans_prs2ruleid,
		Anum_pg_prs2plans_prs2planno,
		0,
		0 },
	  sizeof(FormData_pg_prs2plans) - sizeof(struct varlena),
      NULL,
      NULL  },
    { &RewriteRelationName,			/* RULOID */
	  1,
	  { ObjectIdAttributeNumber,
		0,
		0,
		0 },
	  sizeof(FormData_pg_rewrite) - (2 * sizeof(text)),
      NULL,
      NULL  },
    { &Prs2StubRelationName,			/* PRS2STUB */
	  2,
	  { Anum_pg_prs2stub_prs2relid,
	    Anum_pg_prs2stub_prs2no,
		0,
		0 },
	  sizeof(FormData_pg_prs2stub) - sizeof(struct varlena),
      NULL,
      NULL  },
    { &Prs2RuleRelationName,			/* PRS2RULEID	*/
	  1,
	  { Anum_pg_prs2rule_prs2name,
		0,
		0,
		0 },
	  sizeof(FormData_pg_prs2rule),
      NULL,
      NULL  },
    { &Prs2RuleRelationName,			/* PRS2EVENTREL	*/
	  1,
	  { ObjectIdAttributeNumber,
		0,
		0,
		0 },
	  sizeof(FormData_pg_prs2rule) - sizeof(text),
      NULL,
      NULL  },
    { &AggregateRelationName,			/*AGGNAME*/
	  1,
	  { Anum_pg_aggregate_aggname,
	        0,
		0,
		0 },
	   sizeof(FormData_pg_aggregate) - (2 * sizeof(text)),
       NULL,
       NULL  },    
    { &NamingRelationName,                      /*NAMEREL */
        2,
        { Anum_pg_naming_parent_oid,
            Anum_pg_naming_filename,
            0,
            0 },
        sizeof(FormData_pg_naming),
      &NamingNameIndex,
      NamingNameIndexScan  },
    { &LargeObjectAssocRelationName,           /*LOBJREL */
        1,
        { Anum_pg_large_object_oid,/*LargeObjectOIDAttributeNumber,*/
            0,
            0,
            0 },
        sizeof(FormData_pg_large_object) - sizeof(bytea),
        NULL,
	NULL  },
    { &ListenerRelationName,                  /* LISTENREL */
	2,
	{  Anum_pg_listener_relname,
	     Anum_pg_listener_pid,
	     0,
	     0 },
	sizeof(FormData_pg_listener),
	NULL,
	NULL  },
    { &UserRelationName,           		/* USENAME */
	      1,
	      { Anum_pg_user_usename,
			0,
			0,
			0 },
	      sizeof(FormData_pg_user),
	      NULL,
	      NULL  },
    { &UserRelationName,           		/* USESYSID */
	      1,
	      { Anum_pg_user_usesysid,
			0,
			0,
			0 },
	      sizeof(FormData_pg_user),
	      NULL,
	      NULL  },
    { &GroupRelationName,           		/* GRONAME */
	      1,
	      { Anum_pg_group_groname,
			0,
			0,
			0 },
	      sizeof(FormData_pg_group) - sizeof(int2),
	      NULL,
	      NULL  },
    { &GroupRelationName,           		/* GROSYSID */
	      1,
	      { Anum_pg_group_grosysid,
			0,
			0,
			0 },
	      sizeof(FormData_pg_group) - sizeof(int2),
	      NULL,
	      NULL  },
    { &RewriteRelationName,           		/* REWRITENAME */
	      1,
	      { Anum_pg_rewrite_rulename,
			0,
			0,
			0 },
	      sizeof(FormData_pg_rewrite) - (2 * sizeof(text)),
	      NULL,
	      NULL  }
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
			     cacheinfo[cacheId].indname,
			     cacheinfo[cacheId].nkeys,
			     cacheinfo[cacheId].key,
			     cacheinfo[cacheId].iScanFunc);
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
			     cacheinfo[cacheId].indname,
			     cacheinfo[cacheId].nkeys,
			     cacheinfo[cacheId].key,
			     cacheinfo[cacheId].iScanFunc);
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
	attributeLength = heap_sysattrlen(attributeNumber);
	attributeByValue = heap_sysattrbyval(attributeNumber);
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
	/*
	 * Used to be an elog(DEBUG, ...) here and a claim that it should
	 * be a FATAL error, I don't think either is warranted -mer 6/9/92
	 */
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
    
    typDefault = (struct varlena *)
	SearchSysCacheGetAttribute(TYPOID, 
				   TypeDefaultAttributeNumber,
				   (char *) typId,
				   (char *) NULL,
				   (char *) NULL,
				   (char *) NULL);
    
    if (typDefault == (struct varlena *)NULL) {
#ifdef	CACHEDEBUG
	elog(DEBUG, "TypeDefaultRetrieve: No extractable typdefault",
	     (*cacheinfo[TYPOID].name)->data, TYPOID);
#endif	/* defined(CACHEDEBUG) */
	return(LispNil);
	
    }
    
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
 
