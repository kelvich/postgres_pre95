
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

#include "c.h"
#include "planner/internal.h"
#include "relation.h"
#include "relation.a.h"
#include "planner/costsize.h"
#include "planner/keys.h"

#define _cost_weirdness_ 0


#if (!_cost_weirdness_)    /* global variable */
#define _xprs_ 1
#endif

int _disable_cost_ = 30000000;

#ifndef _xprs_
bool _enable_seqscan_ = true ;
bool _enable_indexscan_ = false; 
bool _enable_sort_ = false;
bool _enable_hash_ = false;,
bool _enable_nestloop_ = true; /* XXX - true */
bool _enable_mergesort_ = false;
bool _enable_hashjoin_ = false;
#endif

#ifdef _xprs_
bool _enable_seqscan_ = true;
bool _enable_indexscan_ = true;
bool _enable_sort_ = true;
bool _enable_hash_ = true;
bool _enable_nestloop_ = true;
bool _enable_mergesort_ = true;  /*   nil in production */
bool _enable_hashjoin_ = true; 
#endif

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
    if ( _cost_weirdness_ ) {
	if ( _enable_seqscan_ ) {
	    temp += 100;
	} 
	else {
	    temp += _disable_cost_;
	} 
    }
    else {
	temp += 0;
    } 
    if(consp (relid)) {
	temp += _TEMP_SCAN_COST_; 	 /*   is this right??? */
    } 
    else {
	temp += relpages;
	temp += _CPU_PAGE_WEIGHT_ * reltuples;
    }
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
	    reltuples,indexpages,indextuples)
     ObjectId indexid;
     int relpages,indexpages;
     int expected_indexpages,selec,indextuples,reltuples;
{
    Cost temp = 0;
    Cost temp2 = 0;
    if ( _cost_weirdness_ ) {
	if ( _enable_indexscan_ ) {
	    temp += 200;
	} 
	else {
	    temp += _disable_cost_;
		} 
    }
    else {
	temp += 0;
    } 
    temp += expected_indexpages;      /*   expected index relation pages */
    temp += selec * indextuples;      /*   about one base relation page */
    /*    per index tuple */
    temp2 += selec * indextuples;
    temp2 += selec * reltuples;
    temp =  temp + (_CPU_PAGE_WEIGHT_ * temp2);
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
     LispValue keys,width,noread ;
     int tuples;
{
    Cost temp = 0;
    
    if ( _cost_weirdness_ ) {
	if ( _enable_sort_ ) 
	  temp += 300;
	else
	  temp += _disable_cost_ ;
	}
    if (tuples == 0 || null(keys) )
      temp += 0;
    else {
	int pages = page_size (tuples,width);
	temp += pages * base_log (pages,2);
	temp += _CPU_PAGE_WEIGHT_ * tuples *base_log (tuples,2);
	if( !noread )
	  temp = temp + cost_seqscan(_TEMP_RELATION_ID_,pages,tuples);
    }
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
    if ( _cost_weirdness_ ) {
	if ( _enable_hash_ ) {
	    temp += 400;
		} 
	else {
	    temp += _disable_cost_;
	} 
    } 
    else 
      temp += 0;
    
    if(tuples == 0 || null(keys)) {
	temp += 0;
    } 
    else {
	/* XXX - let form, maybe incorrect */
		int pages = page_size (tuples,width);
		temp += pages;	    /*   read in */
		temp += _CPU_PAGE_WEIGHT_ * tuples;
		if (_xprs_ && equal(OUTER,which_rel)) {	/*   write out */
		    temp += max (0,pages - _MAX_BUFFERS_);
		} 
		else 
		  temp +=pages;
	}
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
     LispValue width ;
     int tuples;
{
    Cost temp =0;
    temp = temp + page_size(tuples,width);
    temp = temp + _CPU_PAGE_WEIGHT_ * tuples;
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
cost_nestloop (outercost,innercost,outertuples)
     int outercost,innercost,outertuples ;
{
    Cost temp =0;
    if ( _cost_weirdness_ ) {
	if ( _enable_nestloop_ ) 
	  temp += 1000;
		else 
		  temp += _disable_cost_;
    } 
    else 
      temp += 0;
    temp += outercost;
    /*    XXX this is only valid for left-only trees, of course! */
    temp += outertuples * innercost;
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
     LispValue outersortkeys,innersortkeys,outerwidth,innerwidth ;
     int outercost,innercost,outersize,innersize;
{
    Cost temp = 0;
    if ( _cost_weirdness_ ) {
	if ( _enable_mergesort_ ) 
	  temp += 2000;
	else
	  temp += _disable_cost_;
    } 
    else 
      temp += 0;
	
    temp += outercost;
    temp += innercost;
    temp += cost_sort(outersortkeys,outersize,outerwidth,LispNil);
    temp += cost_sort(innersortkeys,innersize,innerwidth,LispNil);
    temp += _CPU_PAGE_WEIGHT_ * (outersize + innersize);
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
     int outercost,innercost;
{
    Cost temp = 0;
	/* XXX - let form, maybe incorrect */
    int outerpages = page_size (outersize,outerwidth);
    int innerpages = page_size (innersize,innerwidth);
    if ( _cost_weirdness_ ) {
	if ( _enable_hashjoin_ ) 
	  temp += 3000;
	else 
	  temp += _disable_cost_;
    } 
    temp += outercost;
    temp += innercost;
    temp += cost_hash(outerkeys,outersize,outerwidth,OUTER);
    temp += cost_hash(innerkeys,innersize,innerwidth,INNER);
    if (_xprs_ == 1)  /* read outer block */
      temp += max(0, outerpages - _MAX_BUFFERS_);
    else
	  temp += outerpages;
    
    temp += outerpages;     /* write join result */
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
     LispValue rel ;
{
	int temp;
	temp = get_tuples(rel) * product_selec(get_clauseinfo(rel));
	return ((int)floor(temp));
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
     LispValue rel ;
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
     int width = get_typlen (get_restype (tl_resdom (tlistentry)));
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
compute_joinrel_size (joinrel)
     Join joinrel ;
{
	int temp = 1;
	temp = temp * get_size(get_parent(get_outerjoinpath(joinrel)));
	temp = temp * get_size(get_parent(get_innerjoinpath(joinrel)));
	temp = temp * product_selec(get_clauseinfo(joinrel));
	return(floor(temp));
	
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
	temp = max(1, (tuples * (width + _TID_SIZE_)) / _PAGE_SIZE_);
	return(temp);
}
