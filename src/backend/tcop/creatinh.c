/*
 * creatinh.c --
 *	POSTGRES create and destroy relation with inheritance utility code.
 */

#include "c.h"

RcsId("$Header$");

#include "log.h"
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
	int		numberOfAttributes;
	TupleDescriptor	desc;

	AssertArg(lispStringp(relationName));
	AssertArg(listp(parameters));
	AssertArg(length(parameters) == 4);
	AssertArg(listp(schema));

	numberOfAttributes = length(schema);

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
#if 0
	desc = createattlist(numberOfAttributes, "schema");
	amcreate(foo(relationName), "is_archived", numberOfAttributes, desc);
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

#if 0
;; NOTE:  If the relation has indices defined on it, then the index
;;        relations will also be destroy, along with the base
;;        relation.

(defun relation-remove (relation-name)
  (delete-inheritance relation-name)
  (check-indices relation-name)
  (am-destroy relation-name)
  (utility-end "DESTROY"))
#endif

#if 0
(defun index-remove (relation-name)
  (delete-index relation-name)
  (am-destroy relation-name)
  (utility-end "REMOVE"))
#endif
