/*
**	relshow  -  display details of a given relation
**
**	Author	: David J. Hughes	(bambi@Bond.edu.au)
**	Date	: Jan - 1993
**	
**	Copyright (C) 1993 David J. Hughes 
**	Permission is granted for unrestricted use of this software, either
**	in part or in full provided that credit is given to the author within
**	the subsequent code and documentation.
*/

#include <stdio.h>
#include <varargs.h>
#include "tmp/libpq.h"


/*
** Note :  I tried to get the attribute name and type name using a join
**		of pg_attribute and pg_type but this failed on all of
**		the catalog relations (but worked on user defined
**		relations).  Either this shows my lack of postquel knowledge
**		or reflects a bug in Postgres 4.0.1 under SunOS 4.1.1 on a
**		Sun3.  The method used is not as efficient as a join (ie. 
**		another	query is sent to the backend) but it does work and is
**		plainly clear for novice database programmers.
**
**		The Mdb* routines are from a database interface library I
**		wrote for the Minerva network management system.  They
**		provide a simple interface to the database for data
**		retrieval by abstracting the query mechanism.  They
**		return a simple linked list of the tuples found during
**		the query (or NULL on error or non-existent tuples).
*/




typedef struct dbData_t {
	char	*data;
	struct	dbData_t *next;
} dbData;

typedef struct dbTable_t {
	char	**data;
	struct	dbTable_t *next;
} dbTable;


#define NOSORT	0
#define	ASCEND	1
#define DESCEND 2


char	*PQexec();
char	*index();
dbData	*MdbCreateValueList();
dbTable	*MdbVaCreateTable();



/****************************************************************************
** 	_MdbCreateValueList 
**
**	Purpose	: Builds a linked list of DB values and hides the complexity
**	Args	: Relation name (char *)
**		  Field name (char *)
**		  Query condition (char *)
**		  Sorting criteria (int)
**	Returns	: Pointer to linked list structure or NULL on error.
**	Notes	: The list returned is malloc'ed during a call to this
**		  routine.  A subsequent call to MdbDestroyValueList()
**		  should be made to free the space when the data is no
**		  longer required.
*/

dbData	*MdbCreateValueList(rel,field,condition,sort)
	char	*rel,
		*field,
		*condition;
	int	sort;
{
	PortalBuffer *p;
	int	index,
		items;
	char	query[200],
		*result;
	dbData	*head,
		*cur;

	/*
	** Snarf the data
	*/

	if (condition)
		(void) sprintf(query,"retrieve (%s.%s) where %s"
			,rel,field,condition);
	else
		(void) sprintf(query,"retrieve (%s.%s)",
			rel,field);

	if (sort)
	{
		strcat(query," sort by ");
		strcat(query,field);
		strcat(query," using ");
		if (sort == ASCEND)
			strcat(query,"<");
		else
			strcat(query,">");
	}
	result = PQexec(query);
	if (*result == 'R')
		return(NULL);

	/*
	** Extract the data and make a list
	*/

	head = cur = NULL;
	p = PQparray(&(result[1]));
	items = PQntuples(p);
	if(!items)
	{
		PQclear(&(result[1]));
		PQexec(" ");
		return(NULL);
	}
	for (index = 0; index < items; index++)
	{
		if (!head)
			head = cur = (dbData *)malloc(sizeof(dbData));
		else
		{
			cur->next = (dbData *)malloc(sizeof(dbData));
			cur = cur->next;
		}
		cur->data = strdup(PQgetvalue(p,index,0));
		cur->next = NULL;
	}
	PQclear(&(result[1]));
	PQexec(" ");
	return(head);
}







/****************************************************************************
** 	_MdbDestroyValueList
**
**	Purpose	: Reclaim the memory used by a value list
**	Args	: Pointer to the value list
**	Returns	: Nothing
**	Notes	: 
*/


MdbDestroyValueList(list)
	dbData	*list;
{
	dbData	*cur;

	cur = list;
	while(cur)
	{
		list = cur;
		cur = cur->next;
		if(list->data)
			free(list->data);
		free(list);
	}
}







/****************************************************************************
** 	_MdbVaCreateTable
**
**	Purpose	: Create a multi entry table from a Postgres query
**	Args	: Relation name (char *)
**		  number of fields (int)
**		  field names (char *, char * ......)
**		  Condition (char *)  (NULL OK)
**		  Field for sorting (char *)  (NULL OK)
**		  Sorting criteria (int)
**	Returns	: Pointer to head of table list.
**	Notes	: Memory is malloc'ed during this routine.  Call
**		  MdbDestroyTable() to free it.
*/

dbTable *MdbVaCreateTable(va_alist)
	va_dcl
{
	PortalBuffer *p;
	va_list	arg;
	char	query[1024],
		tmpBuf[200],
		*rel,
		*field,
		*result,
		*where,
		*sortField;
	int	numFields=0,
		numArgs,
		index,
		items,
		curField,
		sort;
	dbTable	*head,
		*cur;


	/*
	** Snarf the table from the Database
	*/
	va_start(arg);
	rel = va_arg(arg, char *);
	numArgs = va_arg(arg, int);
	sprintf(query,"retrieve (");
	for (index = 0; index < numArgs; index++)
	{
		field = va_arg(arg,char *);
		numFields++;
		sprintf(tmpBuf,"%s.%s,",rel,field);
		strcat(query,tmpBuf);
	}
	query[strlen(query)-1] = ')';
	where = va_arg(arg, char *);
	if (where)
	{
		(void) strcat(query, " where ");
		(void) strcat(query, where);
	}
	sortField = va_arg(arg, char *);
	sort = va_arg(arg, int);
	if (sort)
	{
		if (sortField)
		{
			strcat(query," sort by ");
			strcat(query,sortField);
			strcat(query," using ");
			if (sort == ASCEND)
				strcat(query,"<");
			else
				strcat(query,">");
		}
	}
	result = PQexec(query);
	if (*result == 'R')
		return(NULL);

        /*
        ** Extract the data and build the table
        */

        head = cur = NULL;
        p = PQparray(&(result[1]));
        items = PQntuples(p);
        if(!items)
        {
                PQclear(&(result[1]));
                PQexec(" ");
                return(NULL);
        }

        for (index = 0; index < items; index++)
        {
                if (!head)
		{
                        head = cur = (dbTable *)malloc(sizeof(dbTable));
			head->data = (char **)malloc(sizeof(char *)*numFields);
		}
                else
                {
                        cur->next = (dbTable *)malloc(sizeof(dbTable));
                        cur = cur->next;
			cur->data = (char **)malloc(sizeof(char *)*numFields);
                }
		for (curField = 0; curField < numFields; curField++)
		{
                	*(cur->data + curField) = 
				strdup(PQgetvalue(p,index,curField));
		}
                cur->next = NULL;
        }
        PQclear(&(result[1]));
        PQexec(" ");
        return(head);

}






/****************************************************************************
** 	_MdbDestroyTable
**
**	Purpose	: Reclaim table memory
**	Args	: Pointer to table (dbTable *)
**		  Number of fileds in table (int)
**	Returns	: Nothing
**	Notes	: 
*/

MdbDestroyTable(table,fields)
	dbTable	*table;
	int	fields;
{
	dbTable	*cur;
	int	index;

	cur = table;
	while(cur)
	{
		table = cur;
		cur = cur->next;
		for (index = 0; index < fields; index ++)
		{
			if(table->data[index])
				free(table->data[index]);
		}
		free(table);
	}
}




/****************************************************************************
** 	_usage
**
**	Purpose	: Display usage info
**	Args	: None
**	Returns	: Nothing
**	Notes	: 
*/

usage()
{
	printf("\nUsage : relshow [dbName [relName]]\n\n");
	printf("         Where   dbName is the name of a database\n");
	printf("                 relname is the name of a relation\n\n");
	printf("If no database is given, list the known databases\n");
	printf("If no relation is given, list relations in the database\n");
	printf("If database and relation given, list fields and field types");
	printf(" in the relation\n\n\007");
}






/****************************************************************************
** 	_main
**
**	Purpose	: usual
**	Args	: usual
**	Returns	: Nothing
**	Notes	: 
*/

main(argc,argv)
	int	argc;
	char	*argv[];
{
	char	dbshow = 0,
		relshow = 0,
		attshow = 0;
	int	index,
		items,
		numFields;
	char	cond[200],
		*result,
		oid[10];
	dbData	*data,
		*cur;
	dbTable	*att,
		*curAtt,
		*types,
		*curType;


	/*
	** Work out what we here to do
	*/

	switch(argc)
	{
		case 1:	dbshow++;
			break;
		case 2: relshow++;
			if (*argv[1] == '-')
			{
				usage();
				exit(1);
			}
			break;
		case 3:	attshow++;
			break;
		default:usage();
			exit(1);
	}


	/*
	**  Fire up Postgres
	*/

	if (!dbshow)
		PQsetdb(argv[1]);
	else
		PQsetdb("template1");
	PQexec("begin");



	/*
	** List the availabel databases if required
	*/

	if (dbshow)
	{
		data = MdbCreateValueList("pg_database","datname",NULL,ASCEND);
		if (!data)
		{
			printf("\nCouldn't get database list!  ");
			printf("Have you run initdb?\n\n");
			exit(1);
		}
		printf("\n\nDatabase = postgres (default)\n\n");
		printf("  +-----------------+\n");
		printf("  |    Databases    |\n");
		printf("  +-----------------+\n");
		cur = data;
		while(cur)
		{
			printf("  | %-15.15s |\n", cur->data);
			cur = cur->next;
		}
		printf("  +-----------------+\n\n");
		MdbDestroyValueList(data);
		exit(0);

	}


	/*
	** List the available relations if required
	*/

	if (relshow)
	{

		data = MdbCreateValueList("pg_class","relname",
			"pg_class.relname !~ \"^pg_\"", ASCEND);
		if (!data)
		{
			printf("\nUnable to list relations in database %s ",
				argv[1]);
			printf("(may be empty database)\n\n");
			exit(1);
		}
		printf("\n\nDatabase = %s\n\n",argv[1]);
		printf("  +---------------------+\n");
		printf("  |      Relation       |\n");
		printf("  +---------------------+\n");
		cur = data;
		while(cur)
		{
			printf("  | %-19.19s |\n", cur->data);
			cur = cur->next;
		}
		printf("  +---------------------+\n\n");
		MdbDestroyValueList(data);
		exit(0);
	}


	/*
	** List the attributes and types if required
	*/

	if (attshow)
	{
		/*
		** Get the list of attributes
		*/

		(void)sprintf(cond,"pg_class.relname = \"%s\"",argv[2]);
		data = MdbCreateValueList("pg_class","oid",cond,NOSORT);
		if (!data)
		{
			printf("\nCouldn't find %s in %s!\n\n",
				argv[2], argv[1]);
			exit(1);
		}

		(void)sprintf(cond,"pg_attribute.attrelid = %s::oid and pg_attribute.attnum > 0",
			data->data);
		att = MdbVaCreateTable("pg_attribute",2,"attname","atttypid",
			cond,"attname",ASCEND);
		if (!att)
		{
			printf("Could not retrieve information!\n");
			printf("Database = %s\nRelation = %s\n\n", argv[1],
				argv[2]);
			exit(1);
		}

		/*
		** Get the table of type names and OID's
		*/

		types = MdbVaCreateTable("pg_type",2,"oid","typname",NULL,
			"oid", ASCEND);

		if (!types)
		{
			printf("Could not generate list of valid types ");
			printf("(from pg_type relation)!\n\n");
			exit(1);
		}


		/*
		** Display the information
		*/

		printf("\nDatabase = %s\n",argv[1]);
		printf("\nRelation = %s\n\n",argv[2]);
		printf("  +-----------------+----------+\n");
		printf("  |     Field       |   Type   |\n");
		printf("  +-----------------+----------+\n");
		curAtt = att;
		while(curAtt)
		{
			
			printf("  | %-15.15s | ",curAtt->data[0]);
			
			/*
			** find the type name
			*/
			curType = types;
			while(curType)
			{
				if(strcmp(curType->data[0],curAtt->data[1])==0)
					break;
				curType = curType->next;
			}
			if (curType)
				printf("%-8.8s |\n",curType->data[1]);
			else
				printf("Unknown   |\n");
			curAtt = curAtt->next;
		}
		printf("  +-----------------+----------+\n\n");
	}
}
