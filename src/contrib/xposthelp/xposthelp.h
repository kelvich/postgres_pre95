/*
 * $Header$
 */

/* Basic limits */
#define MAX_DBS  100    /* Maximum number of databases */
#define MAX_REL  1000   /* Maximum number of tables in each database */
#define MAX_ATT  100    /* MAximum number of attributes in a table */

/* Error results */
#define NO_ERROR 0
#define NO_USER  1
#define NO_DBS   2
#define NO_TBS   3

char dbs[MAX_DBS][80],  /* All databases */
     user_name[80],     /* All users     */
     curr_dbase[80];    /* Current database selected */

int user_num,           /* User oid */
    dbs_num,            /* Number of databases */
    tbs_num;            /* Number of tables */
    att_num;            /* Number of attributes */

struct {
  char name[80],        /* Attribute name */
       str_type[80];    /* String attribute type */
  int  type;            /* Integer attribute type */
} att[MAX_ATT];

struct {
  char name[80];        /* Relation name */
  int  kind;            /* 1 if it is a relation 0 if it is an index */
  int  ind;             /* 1 if it is indexed 0 otherwise */
  char oid[20];         /* oid of the relation */
  int  numrec;          /* Number of records */
} tbs[MAX_REL];

#define DESELECT 0
#define SELECT 1
#define ENTRY_KEY 5877
#define NULL_STR ""

