/*
 * $Header$
 */

/**********************************************************
* Getting POSTGRES Table Info. Similar to the command     *
* describe in SQLPLUS (ORACLE).                           *
**********************************************************/

#include "tmp/libpq.h"
#include "utils/geo-decls.h"

#define NUM_DBS  100    /* Maximum number of databases */
#define NUM_REL  1000   /* Maximum number of tables in each database */

char dbs[NUM_DBS][80],  /* All databases */
     user_name[80];     /* All users     */

int user_num,           /* User oid */
    dbs_num,            /* Number of databases */
    tbs_num;            /* Number of tables */

struct {
  char name[80];        /* Relation name */
  int  kind;            /* 1 if it is a relation 0 if it is an index */
  int  ind;             /* 1 if it is indexed 0 otherwise */
  char oid[20];         /* oid of the relation */
  int  numrec;          /* Number of records */
} tbs[NUM_REL];

char *GetIndex();

void main(argc, argv)
     int argc;
     char *argv[];
{
  PortalBuffer *portalbuf;
  char         *res, str[200], attr_name[80];
  int          ngroups,tupno, grpno, ntups, nflds, i, fldno, attr_type, tuple_index, j;
  static int   extensive = 0;

  if (argc > 5)
    {
      printf("Wrong number of arguments. Type: \n\n posthelp {-ext} db_user {db_name} {table_name}\n");
      exit(1);
    }
  else
    if (argc == 1)
      {
	FindAllUsers();
	printf("\nNow you can try: \n\tposthelp <user_name> or \n\tposthelp ext <user_name> or \n\tposthelp <user_name> <db_name> or \n\tposthelp ext <user_name> <db_name> or \n\tposthelp <user_name> <db_name> <table_name>\n\n");
	exit(1);
      }
  else
    if (argc > 1)
      {
	/* check if user wants extensive listing */
	if ((strcmp(argv[1], "ext") == 0) || (strcmp(argv[1], "-ext") == 0))
	  {
	    if (argc == 2)
	      {
		printf(" You have to at least provide an user, i.e.\n\t\tposthelp -ext <user-name>\n\n");
		exit(1);
	      }
	    extensive = 1;
	  }
	else
	  extensive = 0;
      }

  /* Get user name */
  strcpy(user_name, argv[1 + extensive]);
  user_num = FindUser(user_name);

  if (argc == 2 + extensive)
    {
      /* Open up database */
      PQsetdb("postgres");
      
      /* Start up processing */
      PQexec("begin");
      
      /* Define query for this table */
      sprintf(str, "retrieve iportal junk (p.datname) from p in pg_database where p.datdba = \"%d\" sort by datname", user_num);

      res = (char *)PQexec(str);
      if (*res == 'E') 
	{
	  printf("%s\nfailed",++res);
	  goto exit_error;
	}
      
      res = (char *)PQexec("fetch all in junk");
      if (*res != 'P') 
	{
	  printf("\nno portal");
	  goto exit_error;
	}
      
      /* Get portal buffer given portal name. */
      portalbuf = PQparray(++res);
      
      /* Get number of groups in portal buffer */
      ngroups = PQngroups(portalbuf);
      
      for (grpno = 0; grpno < ngroups; grpno++) 
	{
	  /* Get number of tuples in relation */
	  ntups = PQntuplesGroup(portalbuf, grpno);
	  
	  dbs_num = ntups;

	  for (tupno = 0; tupno < ntups; tupno++) 
	    strcpy(dbs[tupno], PQgetvalue(portalbuf,tupno,0));

	}

      PQexec("end");
  
      PQfinish();

      /* Write all tables for each dbase */
      printf("*******\n*******\n\tUSER:     %s (%d) \n\n", user_name, user_num);

      if (!extensive)
	printf("\tDATABASES:\n\n");

      for (j = 0; j < dbs_num; j++)
	{
	  if (extensive)
	    {
	      printf("\tDATABASE:   %s \n\n", dbs[j]);

	      FillTables(dbs[j]);
	      
	      for (i=0; i < tbs_num; i++)
		{
		  if (tbs[i].ind)
		    printf("\tTABLE   = %s\n\tKIND    = %s\n\tINDEXED = yes (%s)\n\tOID     = %s\n\tRECORDS = %d\n", 
			   tbs[i].name, 
			   (tbs[i].kind) ? "relation": "index", 
			   GetIndex(tbs[i].oid), 
			   tbs[i].oid, tbs[i].numrec);
		  else
		    printf("\tTABLE   = %s\n\tKIND    = %s\n\tINDEXED = no\n\tOID     = %s\n\tRECORDS = %d\n", 
			   tbs[i].name, 
			   (tbs[i].kind) ? "relation": "index", 
			   tbs[i].oid, tbs[i].numrec);

		  /* Only print table if it is a relation. No indexes */
		  if (tbs[i].kind)
		    {
		      printf("\n");
		      PrintTable(tbs[i].oid);
		    }
		  else
		    PrintIndexInfo(tbs[i].oid);
		  
		  printf("\n\n");
		}
	    }
	  else
	    printf("\t%s\n", dbs[j]);
	}
    }
  else
    {
      /* fill up only one database */
      dbs_num = 1;
      strcpy(dbs[0], argv[2 + extensive]);

      if (!CheckDBase(dbs[0]))
	{
	  printf("Database \"%s\" does not exist for USER \"%s\"\n", dbs[0], user_name);
	  exit(1);
	}
	

      /* Get all tables in this db */
      if (argc == 3 + extensive)
	{
	  printf("*******\n*******\n\tUSER:   %s (%d) \n\n", user_name, user_num);
	  printf("\tDATABASE:   %s \n\n", dbs[0]);

	  /* Open up database */
	  FillTables(dbs[0]);

	  if (extensive)
	    {
	      for (i=0; i < tbs_num; i++)
		{
		  if (tbs[i].ind)
		    printf("\tTABLE   = %s\n\tKIND    = %s\n\tINDEXED = yes (%s)\n\tOID     = %s\n\tRECORDS = %d\n", 
			   tbs[i].name, 
			   (tbs[i].kind) ? "relation": "index", 
			   GetIndex(tbs[i].oid), 
			   tbs[i].oid, tbs[i].numrec);
		  else
		    printf("\tTABLE   = %s\n\tKIND    = %s\n\tINDEXED = no\n\tOID     = %s\n\tRECORDS = %d\n", 
			   tbs[i].name, 
			   (tbs[i].kind) ? "relation": "index", 
			   tbs[i].oid, tbs[i].numrec);

		  
		  /* Only print table if it is a relation. No indexes */
		  if (tbs[i].kind)
		    {
		      printf("\n");
		      PrintTable(tbs[i].oid);
		    }
		  else
		    PrintIndexInfo(tbs[i].oid);
		  
		  printf("\n\n");
		}
	    }
	  else
	    {
	      printf("\tTABLE\t\t\tKIND\t\tINDEXED\tOID\tRECORDS\n\n"); 
	      
	      for (i=0; i < tbs_num; i++)
		{
		  FillName(tbs[i].name);
		  printf("\t%s\t\t%c\t\t%c\t%s\t%d\n", 
			 tbs[i].name, (tbs[i].kind) ? 'r' : 'i', (tbs[i].ind) ? 'y' : 'n', tbs[i].oid, tbs[i].numrec);
		}
	    }
	}
      else
	{
	  tbs_num = 1;
	  strcpy(tbs[0].name, argv[3 + extensive]);
	  
	  printf("*******\n*******\n\tUSER:   %s (%d) \n\n", user_name, user_num);
	  printf("\tDATABASE:   %s \n\n", dbs[0]);

	  FillOneTable(dbs[0]);

	  if (tbs[0].ind)
	    printf("\tTABLE   = %s\n\tKIND    = %s\n\tINDEXED = yes (%s)\n\tOID     = %s\n\tRECORDS = %d\n", 
		   tbs[0].name, 
		   (tbs[0].kind) ? "relation": "index", 
		   GetIndex(tbs[0].oid), 
		   tbs[0].oid, tbs[0].numrec);
	  else
	    printf("\tTABLE   = %s\n\tKIND    = %s\n\tINDEXED = no\n\tOID     = %s\n\tRECORDS = %d\n", 
		   tbs[0].name, 
		   (tbs[0].kind) ? "relation": "index", 
		   tbs[0].oid, tbs[0].numrec);
	  
	  
	  if (tbs[0].kind) 
	    {
	      printf("\n");
	      PrintTable(tbs[0].oid);
	    }
	  else
	    PrintIndexInfo(tbs[0].oid);
	}

    }
  
  exit(0);
  
 exit_error:
  PQexec("end");
  PQfinish();
  exit(1);
  
}



/*************************************************
 *                                               *
 *                                               *
 *  Given a table name it prints all             *
 *  the attibutes and their types.               *
 *                                               *
 *                                               *
 *                                               *
*************************************************/

PrintTable(tableoid)
     char *tableoid;
{
  PortalBuffer *portalbuf;
  char         *res, str[200], attr_name[80], type_str[80];
  int          ngroups,tupno, grpno, ntups, nflds, i, fldno, attr_type, tuple_index;
  
  /* Start up processing */
  PQexec("begin");
  
  /* Define query for this table */
  sprintf(str, "retrieve iportal junk (p.attname, p.atttypid, p.attnum) from p in pg_attribute where p.attrelid = \"%s\" and p.attnum > 0 sort by attnum", tableoid);

  res = (char *)PQexec(str);
  if (*res == 'E') 
    {
      printf("%s\nfailed",++res);
      goto exit_error;
    }
  
  res = (char *)PQexec("fetch all in junk");
  if (*res != 'P') 
    {
      printf("\nno portal");
      goto exit_error;
    }
  
  /* Get portal buffer given portal name. */
  portalbuf = PQparray(++res);
  
  /* Get number of tuples in relation */
  ntups = PQntuplesGroup(portalbuf, 0);

  /* Print header 
  printf("\t\tAttribute\t\tType\t\t\tNumber\n\n");*/
  printf("\t\tAttribute\t\tType\n\n");
  
  /* Get the attribute name for this group and field number */
  for (i = 0; i < ntups; i++)
    {
      int *tmp;

      strcpy(attr_name, PQgetvalue(portalbuf,i,0));
      tmp = (int *) PQgetvalue(portalbuf,i,1);
      attr_type = *tmp;

      FillName(attr_name);
      
      switch(attr_type)
	{
	case 16:
	  printf("\t\t%s\t\tboolean \n", attr_name);
/*	  printf("\t\t%s\t\tboolean \t\t(%d)\n", attr_name, attr_type);*/
	  break;
	  
	case 18:
	  printf("\t\t%s\t\tchar    \n", attr_name);
/*	  printf("\t\t%s\t\tchar    \t\t(%d)\n", attr_name, attr_type);*/
	  break;
	  
	case 19:
	  printf("\t\t%s\t\tchar16  \n", attr_name);
/*	  printf("\t\t%s\t\tchar16  \t\t(%d)\n", attr_name, attr_type);*/
	  break;
	  
	case 22:
	  printf("\t\t%s\t\tarray   \n", attr_name);
/*	  printf("\t\t%s\t\tarray   \t\t(%d)\n", attr_name, attr_type);*/
	  break;
	  
	case 23:
	  printf("\t\t%s\t\tint4    \n", attr_name);
/*	  printf("\t\t%s\t\tint4    \t\t(%d)\n", attr_name, attr_type);*/
	  break;
	  
	case 25:
	  printf("\t\t%s\t\ttext    \n", attr_name);
/*	  printf("\t\t%s\t\ttext    \t\t(%d)\n", attr_name, attr_type);*/
	  break;
	  
	case 26:
	  printf("\t\t%s\t\toid     \n", attr_name);
/*	  printf("\t\t%s\t\toid     \t\t(%d)\n", attr_name, attr_type);*/
	  break;
	  
	case 600:
	  printf("\t\t%s\t\tpoint    \n", attr_name);
/*	  printf("\t\t%s\t\tpoint    \t\t(%d)\n", attr_name, attr_type);*/
	  break;
	  
	case 601:
	  printf("\t\t%s\t\tlseg    \n", attr_name);
/*	  printf("\t\t%s\t\tlseg    \t\t(%d)\n", attr_name, attr_type);*/
	  break;
	  
	case 602:
	  printf("\t\t%s\t\tpath    \n", attr_name);
/*	  printf("\t\t%s\t\tpath    \t\t(%d)\n", attr_name, attr_type);*/
	  break;
	  
	case 603:
	  printf("\t\t%s\t\tbox     \n", attr_name);
/*	  printf("\t\t%s\t\tbox     \t\t(%d)\n", attr_name, attr_type);*/
	  break;
	  
	case 604:
	  printf("\t\t%s\t\tpolygon \n", attr_name);
/*	  printf("\t\t%s\t\tpolygon \t\t(%d)\n", attr_name, attr_type);*/
	  break;
	  
	case 700:
	  printf("\t\t%s\t\tfloat4  \n", attr_name);
/*	  printf("\t\t%s\t\tfloat4  \t\t(%d)\n", attr_name, attr_type);*/
	  break;
	  
	case 701:
	  printf("\t\t%s\t\tpoint   \n", attr_name);
/*	  printf("\t\t%s\t\tpoint   \t\t(%d)\n", attr_name, attr_type);*/
	  break;
	  
	default:
	  printf("\t\t%s\t\tother   \n", attr_name);
/*	  printf("\t\t%s\t\tother   \t\t(%d)\n", attr_name, attr_type);*/
	  break;
	  
	}/* end switch */
      
    }/* end for loop on fldno */

  if (ntups == 0)
    printf("\t\tEMPTY RELATION\n");

  PQexec("end");
  return;

 exit_error:
  PQexec("end");
  PQfinish();
  exit(1);

}



/*************************************************
 *                                               *
 *                                               *
 *                                               *
 *                                               *
 *                                               *
 *                                               *
 *                                               *
*************************************************/

FillTables(dbname)
     char *dbname;
{
  PortalBuffer *portalbuf;
  char         *res, str[200], attr_name[80];
  int          ngroups,tupno, grpno, ntups, nflds, i, fldno, attr_type, tuple_index, j;
  
  /* Open up database */
  PQsetdb(dbname);
  
  /* Start up processing */
  PQexec("begin");
  
  /* Define query for this table */
  sprintf(str, 
	  "retrieve portal junk (p.relname,p.relkind,p.relhasindex, p.oid, p.reltuples) from p in pg_class where p.relowner = \"%d\" sort by relname", user_num);

  res = (char *)PQexec(str);
  if (*res == 'E') 
    {
      printf("%s\nfailed",++res);
      goto exit_error;
    }
  
  res = (char *)PQexec("fetch all in junk");
  if (*res != 'P') 
    {
      printf("\nno portal");
      goto exit_error;
    }
  
  /* Get portal buffer given portal name. */
  portalbuf = PQparray(++res);
  
  /* Get number of groups in portal buffer */
  ngroups = PQngroups(portalbuf);
  
  for (grpno = 0; grpno < ngroups; grpno++) 
    {
      /* Get number of tuples in relation */
      ntups = PQntuplesGroup(portalbuf, grpno);
      
      tbs_num = ntups;

      for (tupno = 0; tupno < ntups; tupno++) 
	{
	  char *c;
	  int  *tmp;
	  
	  strcpy(tbs[tupno].name, PQgetvalue(portalbuf,tupno,0));

	  c = PQgetvalue(portalbuf,tupno,1);
	  
	  tbs[tupno].kind = (c[0] == 'r') ? 1 : 0;
	  
	  c = (char *)PQgetvalue(portalbuf,tupno,2);
	  
	  tbs[tupno].ind = (c[0] == 't') ? 1 : 0;

	  strcpy(tbs[tupno].oid, PQgetvalue(portalbuf,tupno,3));

	  c = (char *) PQgetvalue(portalbuf,tupno,4);

	  
	  tbs[tupno].numrec = atoi(c);
	  
	}
    }
  
  PQexec("end");
  
  PQfinish();

  return;

 exit_error:
  PQexec("end");
  PQfinish();
  exit(1);

}



/*************************************************
 *                                               *
 *                                               *
 *                                               *
 *                                               *
 *                                               *
 *                                               *
 *                                               *
*************************************************/

FillOneTable(dbname)
     char *dbname;
{
  PortalBuffer *portalbuf;
  char         *res, str[200], attr_name[80], *c;
  int          ngroups,tupno, grpno, ntups, nflds, i, fldno, attr_type, tuple_index, j;
  
  /* Open up database */
  PQsetdb(dbname);
  
  /* Start up processing */
  PQexec("begin");
  
  /* Define query for this table */
  sprintf(str, 
	  "retrieve portal junk (p.relname, p.relkind,p.relhasindex, p.oid, p.reltuples) from p in pg_class where p.relname = \"%s\"", tbs[0].name);

  res = (char *)PQexec(str);
  if (*res == 'E') 
    {
      printf("%s\nfailed",++res);
      goto exit_error;
    }
  
  res = (char *)PQexec("fetch all in junk");
  if (*res != 'P') 
    {
      printf("\nno portal");
      goto exit_error;
    }
  
  /* Get portal buffer given portal name. */
  portalbuf = PQparray(++res);
  
  /* Get number of groups in portal buffer */
  ngroups = PQngroups(portalbuf);
  
  for (grpno = 0; grpno < ngroups; grpno++) 
    {
      /* Get number of tuples in relation */
      ntups = PQntuplesGroup(portalbuf, grpno);
       
      if (ntups)
	{
	  char *c;
	  int  *tmp;
	  
	  strcpy(tbs[0].name, PQgetvalue(portalbuf,0,0));

	  c = PQgetvalue(portalbuf,0,1);
	  
	  tbs[0].kind = (c[0] == 'r') ? 1 : 0;
	  
	  c = (char *)PQgetvalue(portalbuf,0,2);
	  
	  tbs[0].ind = (c[0] == 't') ? 1 : 0;

	  strcpy(tbs[0].oid, PQgetvalue(portalbuf,0,3));

	  c = (char *) PQgetvalue(portalbuf,0,4);
	  
	  tbs[tupno].numrec = atoi(c);
	  
	}
      else
	{
	  printf(" Error: table %s does not exist in database %s.\n", tbs[0].name, dbname);
	  goto exit_error;
	}
    }

  

  PQexec("end");
  
  PQfinish();

  return;

 exit_error:
  PQexec("end");
  PQfinish();
  exit(1);

}



FillName(attr_name)
     char *attr_name;
{
  int length, i;

  /* Fill up attr_name if less than 8 characters */
  length = strlen(attr_name);

  if (length <9)
    {
      for (i = length; i < 9; i++)
	attr_name[i] = ' ';

      attr_name[9] = '\0';

    }

}






/*************************************************
 *                                               *
 *                                               *
 *  Given an indexed table name it prints all    *
 *  the index information.                       *
 *                                               *
 *  In pg_index the attributes:                  *
 *                                               *
 *  indexrelid stands for the oid of the index   *
 *             relation.                         *
 *                                               *
 *  indrelid is the oid of the relation indexed. *
 *                                               *
*************************************************/

PrintIndexInfo(indexoid)
     char *indexoid; /* oid of the index relation. More than one key on this index */
{
  PortalBuffer *portalbuf;
  char         *res, str[200], relname[80];
  int          ntups, i, relid, numkeys;
  struct {
    int key;
    char attname[20];
  } keys[10];

  /* Start up processing */
  PQexec("begin");
  
  /****************************************
  * First let's find out the relation oid *
  * that was indexed by indexoid and the  *
  * correpondent index keys               *
  ****************************************/

  /* Define query for this table */
  sprintf(str,"retrieve portal junk (p.indrelid, p.indkey[1],p.indkey[2],p.indkey[3],p.indkey[4],p.indkey[5],p.indkey[6],p.indkey[7],p.indkey[8]) from p in pg_index where p.indexrelid = \"%s\"", indexoid);

  res = (char *)PQexec(str);
  if (*res == 'E') 
    {
      printf("%s\nfailed",++res);
      goto exit_error;
    }
  
  res = (char *)PQexec("fetch all in junk");
  if (*res != 'P') 
    {
      printf("\nno portal");
      goto exit_error;
    }
  
  /* Get portal buffer given portal name. */
  portalbuf = PQparray(++res);
  
  /* Get number of tuples in relation */
  ntups = PQntuplesGroup(portalbuf, 0);

  if (ntups)
    {
      char *tmp;

      tmp = (char *) PQgetvalue(portalbuf,0,0);

      /* relation oid on which the index is defined */
      relid = atoi(tmp);

      for (i = 1; i < 9; i++)
	{
	  /* Get each key attribute number */
	  tmp = (char *) PQgetvalue(portalbuf,0,i);

	  if (atoi(tmp) == 0)
	    break;

	  keys[i-1].key = atoi(tmp);

	}

      numkeys = i-1;

      /******************************************************
      *  Now for each key number and for the relation oid   *
      *  get the relation name and the attribute name       *
      ******************************************************/
      for (i = 0; i < numkeys; i++)
	{
	  PQexec("end");
	  PQfinish();
	  
	  /* Start up processing */
	  PQexec("begin");
  
	  /* Define query for this table */
	  sprintf(str,"retrieve portal junk (p.relname, pg_attribute.attname) from p in pg_class where p.oid = \"%d\" and pg_attribute.attnum = \"%d\" and pg_attribute.attrelid = \"%d\"", relid, keys[i].key, relid);

	  res = (char *)PQexec(str);
	  if (*res == 'E') 
	    {
	      printf("%s\nfailed",++res);
	      goto exit_error;
	    }
	  
	  res = (char *)PQexec("fetch all in junk");
	  if (*res != 'P') 
	    {
	      printf("\nno portal");
	      goto exit_error;
	    }
	  
	  /* Get portal buffer given portal name. */
	  portalbuf = PQparray(++res);
	  
	  /* Get number of tuples in relation */
	  ntups = PQntuplesGroup(portalbuf, 0);
	  
	  if (ntups)
	    {
	      strcpy(keys[i].attname, PQgetvalue(portalbuf,0,1));
	      strcpy(relname, PQgetvalue(portalbuf,0,0));
	    }
	}
      
      /* Put out info on relation name and key attribute names */
      printf("\tINDEX ON= %s \n", relname);

      printf("\tKEYS    = "); 
      for (i=0; i< numkeys; i++)
	printf("%s  ", keys[i].attname);

      printf("\n");
	  

    }

  PQexec("end");
  PQfinish();
  return;

 exit_error:
  PQexec("end");
  PQfinish();
  exit(1);




}




/*************************************************
 *                                               *
 *  Find all users in postgres system.           *
 *                                               *
*************************************************/

int FindAllUsers()
{
  PortalBuffer *portalbuf;
  char         *res, str[200];
  int          ntups, result, i;
  
  /* Open up database */
  PQsetdb("postgres");
  
  /* Start up processing */
  PQexec("begin");
  
  /* Define query for this table */
  strcpy(str,"retrieve portal junk (p.usename) from p in pg_user ");

  res = (char *)PQexec(str);
  if (*res == 'E') 
    {
      printf("%s\nfailed",++res);
      goto exit_error;
    }
  
  res = (char *)PQexec("fetch all in junk");
  if (*res != 'P') 
    {
      printf("\nno portal");
      goto exit_error;
    }
  
  /* Get portal buffer given portal name. */
  portalbuf = PQparray(++res);
  
  /* Get number of tuples in relation */
  ntups = PQntuplesGroup(portalbuf, 0);
      
  printf("\tUSERS: \n");

  for (i = 0; i < ntups; i++)
    printf("\t\t%s\n", PQgetvalue(portalbuf,i,0));

  PQexec("end");
  PQfinish();
  return;

 exit_error:
  PQexec("end");
  PQfinish();
  exit(1);

}




/*************************************************
 *                                               *
 *  Check if the given dbase exists in the       * 
 *  postgres system.                             *
 *  Returns TRUE if it exists FALSE otherwise.   *
 *                                               *
*************************************************/

int CheckDBase(dbname)
     char *dbname;
{
  PortalBuffer *portalbuf;
  char         *res, str[200];
  int          ntups, result, i;
  
  /* Open up database */
  PQsetdb("postgres");
  
  /* Start up processing */
  PQexec("begin");
  
  /* Define query for this table */
  sprintf(str,"retrieve portal junk (p.datname) from p in pg_database where p.datdba = \"%d\" and p.datname = \"%s\" ", user_num, dbname);

  res = (char *)PQexec(str);
  if (*res == 'E') 
    {
      printf("%s\nfailed",++res);
      goto exit_error;
    }
  
  res = (char *)PQexec("fetch all in junk");
  if (*res != 'P') 
    {
      printf("\nno portal");
      goto exit_error;
    }
  
  /* Get portal buffer given portal name. */
  portalbuf = PQparray(++res);
  
  /* Get number of tuples in relation */
  ntups = PQntuplesGroup(portalbuf, 0);
      
  PQexec("end");
  PQfinish();
  return(ntups);

 exit_error:
  PQexec("end");
  PQfinish();
  exit(1);

}
