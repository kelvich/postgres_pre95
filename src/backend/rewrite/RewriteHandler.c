/*
 *	QueryRewrite
 *	- takes a parsetree, if there are no locks at all, return NULL
 *	  if there are locks, evaluate until no locks exist, then
 *	  return list of action-parsetrees, 
 *	$Header$
 */
#ifdef nowrk
QueryRewrite ( parsetree )
     List parsetree;
{
    List root		= NULL;
    List rangetable     = NULL;
    int  command	= 0;
    int	 varno		= 1;
    List parselist	= NULL;

    if ((root = parse_root(parsetree)) != NULL ) {
	rangetable = root_rangetable(root);
	command = root_command_type ( root );
    }

    foreach ( i , rangetable ) {
	List entry = CAR(i);
	Name varname = CString(CAR(entry));
	Relation to_be_rewritten = RelationNameGetRelation ( varname );

	printf("checking for locks on %s\n",varname);
	fflush(stdout);
	
	if ( RelationHasLocks( to_be_rewritten )) {
	    parselist = nappend(parselist, 
				HandleRewrite ( parsetree , to_be_rewritten ));
	}
	varno += 1;
    }
    return parselist;
}

/*
 *	HandleRewrite
 *	- initial implementation, only do instead
 *
 *	if there exists > 1 do_instead rule pending on a relation.attr 
 *		error
 *	[ there exists at most 1 do instead rule ]
 *	for each pending rule, generate a string corresponding to the action
 *
 *	
 *	if there exists a do instead rule, discard original query
 */

HandleRewrite ( parsetree )
     List parsetree;
{
    
}


/*
 *	RelationHasLocks
 *	- returns true iff a relation has some locks on it
 */

bool
RelationHasLocks ( relation )
     Relation relation;
{
    HeapTuple relationTuple;
    RuleLock relationLock;
    RuleLock tupleLock;
    RuleLockIntermediate *theRelationRuleLocks;
    RuleLockIntermediate *theTupleRuleLocks;
    RuleLockIntermediate *theNewRuleLocks;
    Datum datum;
    Boolean isnull;


    relationTuple = RelationGetRelationTupleForm ( relation );
    datum = HeapTupleGetAttributeValue(
		relationTuple,
		InvalidBuffer,
		(AttributeNumber) -2,
		(TupleDescriptor) NULL,
		&isnull);

    relationLock = (RuleLock)DatumGetPointer(datum);
    theRelationRuleLocks = RuleLockInternalToIntermediate(relationLock);

    if ( theRelationRuleLocks == NULL )
      return (false );
    else
      return ( true );
    
}

#endif

