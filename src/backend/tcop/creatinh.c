/*
 * creatinh.c --
 *	POSTGRES create/destroy relation with inheritance utility code.
 */

#include "c.h"

RcsId("$Header$");

#include "attnum.h"
#include "log.h"
#include "name.h"
#include "pg_lisp.h"
#include "tupdesc.h"

/*
 * CREATE relation_name '(' dom_list ')' [KEY '(' dom_name [USING operator] 
 * 	{ , '(' dom_name [USING operator] } ')']
 * 	[INHERITS '(' relation_name {, relation_name} ')' ] 
 * 	[INDEXABLE dom_name { ',' dom_name } ] 
 * 	[ARCHIVE '=' (HEAVY | LIGHT | NONE)]
 * 
 * 	`(CREATE ,relation_name
 * 		 (
 * 		  (KEY (,domname1 ,operator1) (,domname2 ,operator2) ...)
 * 		  (INHERITS ,relname1 ,relname2 ...)
 * 		  (INDEXABLE ,idomname1 ,idomname2 ...)
 * 		  (ARCHIVE ,archive)
 * 		 )
 * 		 (,attname1 ,typname1) (,attname2 ,typname2) ...)
 * 
 * 	... where	operator?	is either ,operator or 'nil
 * 			archive		is one of 'HEAVY, 'LIGHT, or 'NONE
 * 
 */

void
DefineRelation(relationName, parameters, schema)
	LispValue	relationName;
	LispValue	parameters;
	LispValue	schema;
{
	AttributeNumber	numberOfAttributes;
	AttributeNumber	attributeNumber;
	TupleDescriptor	desc;
	LispValue	rest;
	ObjectId	relationId;

	AssertArg(lispStringp(relationName));
	AssertArg(listp(parameters));
	AssertArg(length(parameters) == 4);
	AssertArg(listp(schema));

	numberOfAttributes = length(schema);

	/*
	 * Handle parameters
	 * XXX parameter handling missing below.
	 */
	if (!lispNullp(CAR(parameters))) {
		elog(WARN, "RelationCreate: parameters too complex 1");
	}
	if (!lispNullp(CADR(parameters))) {
		elog(WARN, "RelationCreate: parameters too complex 2");
	}
	if (!lispNullp(CADDR(parameters))) {
		elog(WARN, "RelationCreate: parameters too complex 3");
	}
	if (!lispNullp(CADDR(CDR(parameters)))) {
		elog(WARN, "RelationCreate: parameters too complex 4");
	}

	/*
	 * Merge inherited attributes into schema.
	 */

	/*
	 * build tuple descriptor for the heap create routine
	 */
	desc = CreateTemplateTupleDesc(numberOfAttributes);
	attributeNumber = 0;
	foreach(rest, schema) {
		LispValue	elem;

		attributeNumber += 1;
		/* get '("attname" "typname") */
		elem = CAR(rest);
#ifndef	PERFECTPARSER
		AssertArg(listp(elem));
		AssertArg(listp(CDR(elem)));
		AssertArg(lispStringp(CAR(elem)));
		AssertArg(lispStringp(CADR(elem)));
#endif
		TupleDescInitEntry(desc, attributeNumber,
			CString(CAR(elem)), CString(CADR(elem)));
	}

	relationId = RelationNameCreateHeapRelation(CString(relationName),
		/*
		 * Default archive mode should be looked up from the VARIABLE
		 * relation the first time.  Note that it is set to none, now.
		 */
		'n',	/* XXX use symbolic constant ... */
		numberOfAttributes, desc);

	/*
	 * Catalog inherited attribute information.
	 */
#if 0
	EndUtility("CREATE");
#endif
}

#if 0
(defun relation-define (relation-name parameters schema)
  ;; relation-name = (car (cdr CREATE))
  ;; parameters    = (cadr (cdr CREATE))	= ( (KEY ...) ... )
  ;; schema	   = (cddr (cdr CREATE))	= ( (attname1 typname1) ... )
  (let* ((supers (assoc 'INHERITS parameters))
	 (schema (if (null supers)
		   schema
		   (merge-attributes relation-name schema (cdr supers))))
	 (number-of-attributes (length schema))
	 ;; amcreate(rel_name, is_archived, n_attributes, attribute-ptrs)
	 (relation-id
	  (am-create relation-name
		     (archive-mode->c-archive-mode (cadr (assoc 'ARCHIVE
								parameters)))
		     number-of-attributes
		     ;; createattlist(n_attributes, name1, type1, ...)
		     (apply #'createattlist
			    (cons number-of-attributes
				  (apply #'append schema))))))
    (if (not (null supers))
      (add-inheritance relation-id relation-name (cdr supers))))
  (utility-end "CREATE"))
#endif

void
RemoveRelation(name)
	Name	name;
{
	AssertArg(NameIsValid(name));

	/* (delete-inheritance relation-name) */
	/* (check-indices relation-name) */
	RelationNameDestroyHeapRelation(name);
#if 0
	EndUtility("DESTROY");
#endif
}

#if 0
(defun index-remove (relation-name)
  (delete-index relation-name)
  (am-destroy relation-name)
  (utility-end "REMOVE"))
#endif
