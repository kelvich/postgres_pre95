/*     
 *      FILE
 *     	costsize
 *     
 *      DESCRIPTION
 *     	Routines to compute (and set) relation sizes and path costs
 *     
 */
/* RcsId("$Header$"); */

/*     
 *      EXPORTS
 *     		cost_seqscan
 *     		cost_index
 *     		cost_sort
 *     		cost_hash
 *     		cost_nestloop
 *     		cost_mergesort
 *     		cost_hashjoin
 *     		compute-rel-size
 *     		compute-rel-width
 *     		compute-targetlist-width
 *     		compute-joinrel-size
 */

#include <math.h>
#include <values.h>

#include "tmp/c.h"

#include "nodes/relation.h"
#include "nodes/relation.a.h"

#include "planner/internal.h"
#include "planner/costsize.h"
#include "planner/keys.h"
#include "planner/clausesel.h"
#include "planner/tlist.h"
#include "storage/bufmgr.h"	/* for BLCKSZ */

/*
 * CostAddCount --
 *	_cost_ += _count_;
 * CostAddCostTimesCount --
 *	_cost1_ += _cost2_ * _count_;
 * CostMultiplyCount --
 *	_cost_ *= _count_;
 *
 * Macros to circumvent Sun's brain-damaged cc.
 */
#define	CostAddCount(_cost_, _count_) \
	{ Cost cost = _count_; _cost_ += cost; }

#define	CostAddCostTimesCount(_cost1_, _cost2_, _count_) \
	{ Cost cost = _count_; _cost1_ += _cost2_ * cost; }

#define	CostMultiplyCount(_cost_, _count_) \
	{ Cost cost = _count_; _cost_ *= cost; }

int _disable_cost_ = 30000000;
 
bool _enable_seqscan_ =     true;
bool _enable_indexscan_ =   true;
bool _enable_sort_ =        true;
bool _enable_hash_ =        true;
bool _enable_nestloop_ =    true;
bool _enable_mergesort_ =   true;
bool _enable_hashjoin_ =    true;

/*    
 *    	cost_seqscan
 *    
 *    	Determines and returns the cost of scanning a relation sequentially.
 *    	If the relation is a temporary to be materialized from a query
 *    	embedded within a data field (determined by 'relid' containing an
 *    	attribute reference), then a predetermined constant is returned (we
 *    	have NO IDEA how big the result of a POSTQUEL procedure is going to
 *    	be).
 *    
 *    		disk = p
 *    		cpu = *CPU-PAGE-WEIGHT* * t
 *    
 *    	'relid' is the relid of the relation to be scanned
 *    	'relpages' is the number of pages in the relation to be scanned
 *    		(as determined from the system catalogs)
 *    	'reltuples' is the number of tuples in the relation to be scanned
 *    
 *    	Returns a flonum.
 *    
 */

/*  .. cost_hash, cost_sort, create_seqscan_path  */

Cost
cost_seqscan (relid,relpages,reltuples)
     LispValue relid;
     int reltuples, relpages;
{
    Cost temp = 0;
    if ( !_enable_seqscan_ )
	temp += _disable_cost_;
    if(consp (relid)) {
	temp += _TEMP_SCAN_COST_; 	 /*   is this right??? */
    } 
    else {
	temp += relpages;
	temp += _CPU_PAGE_WEIGHT_ * reltuples;
    }
    Assert(temp >= 0);
    return(temp);
} /* end cost_seqscan */


/*    
 *    	cost_index
 *    
 *    	Determines and returns the cost of scanning a relation using an index.
 *    
 *    		disk = expected-index-pages + expected-data-pages
 *    		cpu = *CPU-PAGE-WEIGHT* *
 *    			(expected-index-tuples + expected-data-tuples)
 *    
 *    	'indexid' is the index OID
 *    	'expected-indexpages' is the number of index pages examined in the scan
 *    	'selec' is the selectivity of the index
 *    	'relpages' is the number of pages in the main relation
 *    	'reltuples' is the number of tuples in the main relation
 *    	'indexpages' is the number of pages in the index relation
 *    	'indextuples' is the number of tuples in the index relation
 *    
 *    	Returns a flonum.
 *    
 */

/*  .. best-or-subclause-index, create_index_path, index-innerjoin  */
Cost
cost_index (indexid,expected_indexpages,selec,relpages,
	    reltuples,indexpages,indextuples, is_injoin)
	ObjectId indexid;
	Count expected_indexpages;
	Cost selec;
	Count relpages,indexpages,indextuples,reltuples;
	bool is_injoin;
{
	Cost temp;
	Cost temp2;

	temp = temp2 = (Cost) 0;

	if (!_enable_indexscan_ && !is_injoin)
	    temp += _disable_cost_;

	CostAddCount(temp, expected_indexpages);
				/*   expected index relation pages */

	CostAddCount(temp, MIN(relpages,(int)ceil((double)selec*indextuples)));
				/*   about one base relation page */
	/*
	 * per index tuple
	 */
	CostAddCostTimesCount(temp2, selec, indextuples);
	CostAddCostTimesCount(temp2, selec, reltuples);

	temp =  temp + (_CPU_PAGE_WEIGHT_ * temp2);
	Assert(temp >= 0);
	return(temp);
} /* end cost_index */

/*    
 *    	cost_sort
 *    
 *    	Determines and returns the cost of sorting a relation by considering
 *    	1. the cost of doing an external sort:	XXX this is probably too low
 *    		disk = (p lg p)
 *    		cpu = *CPU-PAGE-WEIGHT* * (t lg t)
 *    	2. the cost of reading the sort result into memory (another seqscan)
 *    	   unless 'noread' is set
 *    
 *    	'keys' is a list of sort keys
 *    	'tuples' is the number of tuples in the relation
 *    	'width' is the average tuple width in bytes
 *    	'noread' is a flag indicating that the sort result can remain on disk
 *    		(i.e., the sort result is the result relation)
 *    
 *    	Returns a flonum.
 *    
 */

/*  .. cost_mergesort, match-unsorted-inner, match-unsorted-outer
 *  .. sort-relation-paths
 */
Cost
cost_sort (keys,tuples,width,noread)
     LispValue keys;
     int tuples;
     int width;
     bool noread;
{
    Cost temp = 0;
    int npages = page_size (tuples,width);
    Cost pages = (Cost)npages;
    Cost numTuples = tuples;
    
    if ( !_enable_sort_ ) 
      temp += _disable_cost_ ;
    if (tuples == 0 || null(keys) )
     {
	 Assert(temp >= 0);
	 return(temp);
     }
    temp += pages * base_log((double)pages, (double)2.0);
    /* could be base_log(pages, NBuffers), but we are only doing 2-way merges */
    temp += _CPU_PAGE_WEIGHT_ * numTuples * base_log((double)pages,(double)2.0);
    if( !noread )
      temp = temp + cost_seqscan(lispInteger(_TEMP_RELATION_ID_),npages,tuples);
    Assert(temp >= 0);
    return(temp);
}


/*    
 *    	cost_hash		XXX HASH
 *    
 *    	Determines and returns the cost of hashing a relation by considering
 *    	1. the cost of doing a hash:
 *    		disk = ???
 *    		cpu = *CPU-PAGE-WEIGHT* * ???
 *    	2. the cost of reading the hash result into memory (another seqscan)
 *    	   unless 'is-outer' is set
 *    
 *    	'keys' is a list of hash keys
 *    	'tuples' is the number of tuples in the relation
 *    	'width' is the average tuple width in bytes
 *    	'which-rel' indicates which (outer or inner) relation this is
 *    
 *    	Returns a flonum.
 *    
 */

/*  .. cost_hashjoin  */

Cost
cost_hash (keys,tuples,width,which_rel)
     LispValue keys;
     int tuples, width, which_rel;  /* which_rel is a #defined const */
{
    Cost temp = 0;
    if ( !_enable_hash_ )
	temp += _disable_cost_;
    if(tuples == 0 || null(keys)) {
	temp += 0;
    } 
    else {
	/* XXX - let form, maybe incorrect */
		int pages = page_size (tuples,width);
		temp += pages;	    /*   read in */
		temp += _CPU_PAGE_WEIGHT_ * tuples;
		if ((OUTER == which_rel)) {	/*   write out */
		    temp += max (0,pages - NBuffers);
		} 
		else 
		  temp +=pages;
	}
    Assert(temp >= 0);
    return(temp);
} /* end cost_hash */

/*    
 *    	cost_result
 *    
 *    	Determines and returns the cost of writing a relation of 'tuples'
 *    	tuples of 'width' bytes out to a result relation.
 *    
 *    	Returns a flonum.
 *    
 */

/*  .. sort-relation-paths
 */
Cost
cost_result (tuples,width)
     int tuples;
     int width ;
{
    Cost temp =0;
    temp = temp + page_size(tuples,width);
    temp = temp + _CPU_PAGE_WEIGHT_ * tuples;
    Assert(temp >= 0);
    return(temp);
}

/*    
 *    	cost_nestloop
 *    
 *    	Determines and returns the cost of joining two relations using the 
 *    	nested loop algorithm.
 *    
 *    	'outercost' is the (disk+cpu) cost of scanning the outer relation
 *    	'innercost' is the (disk+cpu) cost of scanning the inner relation
 *    	'outertuples' is the number of tuples in the outer relation
 *    
 *    	Returns a flonum.
 *    
 */

/*  .. create_nestloop_path  */

Cost
cost_nestloop (outercost,innercost,outertuples,innertuples,outerpages,is_indexjoin)
     Cost outercost,innercost;
     Count outertuples, innertuples ;
     Count outerpages;
     bool is_indexjoin;
{
    Cost temp =0;

    if ( !_enable_nestloop_ ) 
       temp += _disable_cost_;
    temp += outercost;
    temp += outertuples * innercost;
    Assert(temp >= 0);
    return(temp);
}

/*    
 *    	cost_mergesort
 *    
 *    	'outercost' and 'innercost' are the (disk+cpu) costs of scanning the
 *    		outer and inner relations
 *    	'outersortkeys' and 'innersortkeys' are lists of the keys to be used
 *    		to sort the outer and inner relations
 *    	'outertuples' and 'innertuples' are the number of tuples in the outer
 *    		and inner relations
 *    	'outerwidth' and 'innerwidth' are the (typical) widths (in bytes)
 *    		of the tuples of the outer and inner relations
 *    
 *    	Returns a flonum.
 *    
 */

/*  .. create_mergesort_path */

Cost
cost_mergesort (outercost,innercost,outersortkeys,innersortkeys,
		outersize,innersize,outerwidth,innerwidth)
     LispValue outersortkeys,innersortkeys;
     Cost outercost,innercost;
     int outersize,innersize,outerwidth,innerwidth;
{
    Cost temp = 0;
    if ( !_enable_mergesort_ ) 
      temp += _disable_cost_;
	
    temp += outercost;
    temp += innercost;
    temp += cost_sort(outersortkeys,outersize,outerwidth,false);
    temp += cost_sort(innersortkeys,innersize,innerwidth,false);
    temp += _CPU_PAGE_WEIGHT_ * (outersize + innersize);
    Assert(temp >= 0);
    return(temp);
}

/*    
 *    	cost_hashjoin		XXX HASH
 *    
 *    	'outercost' and 'innercost' are the (disk+cpu) costs of scanning the
 *    		outer and inner relations
 *    	'outerkeys' and 'innerkeys' are lists of the keys to be used
 *    		to hash the outer and inner relations
 *    	'outersize' and 'innersize' are the number of tuples in the outer
 *    		and inner relations
 *    	'outerwidth' and 'innerwidth' are the (typical) widths (in bytes)
 *    		of the tuples of the outer and inner relations
 *    
 *    	Returns a flonum.
 *    
 */

/*  .. create_hashjoin_path */

Cost
cost_hashjoin (outercost,innercost,outerkeys,innerkeys,outersize,
	       innersize,outerwidth,innerwidth)
     LispValue outerkeys,innerkeys;
     int outersize, innersize, outerwidth, innerwidth;
     Cost outercost,innercost;
{
    Cost temp = 0;
    /* XXX - let form, maybe incorrect */
    int outerpages = page_size (outersize,outerwidth);
    int innerpages = page_size (innersize,innerwidth);
    int nrun = ceil((double)outerpages/(double)NBuffers);

    if (outerpages < innerpages)
       return _disable_cost_;
    if ( !_enable_hashjoin_ ) 
       temp += _disable_cost_;
    temp += outercost + nrun * innercost;
    temp += _CPU_PAGE_WEIGHT_ * (outersize + nrun * innersize);
    Assert(temp >= 0);
    return(temp);
    
} /* end cost_hashjoin */

/*    
 *    	compute-rel-size
 *    
 *    	Computes the size of each relation in 'rel-list' (after applying 
 *    	restrictions), by multiplying the selectivity of each restriction 
 *    	by the original size of the relation.  
 *    
 *    	Sets the 'size' field for each relation entry with this computed size.
 *    
 *      Returns the size.
 *    
 */

/*  .. find-paths */

int
compute_rel_size (rel)
     Rel rel ;
{
    Cost temp;
    int temp1;

    temp = get_tuples(rel) * product_selec(get_clauseinfo(rel)); 
    Assert(temp >= 0);
    if (temp >= (MAXINT - 1)) {
	temp1 = MAXINT;
    } else {
	temp1 = ceil((double) temp);
    }
    Assert(temp1 >= 0);
    Assert(temp1 <= MAXINT);
    return(temp1);
      
}

/*    
 *    	compute-rel-width
 *    
 *    	Computes the width in bytes of a tuple from 'rel'.
 *    
 *    	Returns the width of the tuple as a fixnum.
 *    
 */

/*  .. find-join-paths, find-paths */

int
compute_rel_width (rel)
     Rel rel ;
{

     return (compute_targetlist_width(get_actual_tlist(get_targetlist (rel))));
}

/*    
 *    	compute-targetlist-width
 *    
 *    	Computes the width in bytes of a tuple made from 'targetlist'.
 *    
 *    	Returns the width of the tuple as a fixnum.
 *    
 */

/*  .. compute-rel-width, subplanner */

int
compute_targetlist_width (targetlist)
     LispValue targetlist ;
{
	LispValue temp_tl;
	int tuple_width = 0;
	foreach ( temp_tl,targetlist) {
	     tuple_width = tuple_width + 
	       compute_attribute_width(CAR(temp_tl));
	     /* XXX - may be wrong */
	}
	return(tuple_width);
}

/*    
 *    	compute-attribute-width
 *    
 *    	Given a target list entry, find the size in bytes of the attribute.
 *    
 *    	If a field is variable-length, it is assumed to be at least the size
 *    	of a TID field.
 *    
 *    	Returns the width of the attribute as a fixnum.
 *    
 */

/*  .. compute-targetlist-width  */

int
compute_attribute_width (tlistentry)
     LispValue tlistentry ;
{
     int width = get_typlen (get_restype ((Resdom)tl_resdom (tlistentry)));
     if ( width < 0 ) 
       return(_DEFAULT_ATTRIBUTE_WIDTH_);
     else 
       return(width);
}

/*    
 *    	compute-joinrel-size
 *    
 *    	Computes the size of the join relation 'joinrel'.
 *    
 *    	Returns a fixnum.
 *    
 */

/*  .. prune-rel-paths */

int
compute_joinrel_size (joinpath)
     JoinPath joinpath ;
{
    Cost temp = 1.0;
    Count temp1 = 0;

    CostMultiplyCount(temp, get_size(get_parent((Path)get_outerjoinpath(joinpath))));
    CostMultiplyCount(temp, get_size(get_parent((Path)get_innerjoinpath(joinpath))));
    temp = temp * product_selec(get_pathclauseinfo(joinpath));  

    temp1 = floor((double)temp);
    Assert(temp1 >= 0);
    return(temp1);
}

/*    
 *    	page-size
 *    
 *    	Returns an estimate of the number of pages covered by a given
 *    	number of tuples of a given width (size in bytes).
 *    
 */
int
page_size (tuples,width)
     int tuples,width ;
{
	int temp =0;
	temp = ceil((double)(tuples * (width + sizeof(HeapTupleData))) 
		    / BLCKSZ);
	Assert(temp >= 0);
	return(temp);
}
