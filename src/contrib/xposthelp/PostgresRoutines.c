/*
 * $Header$
 */

#include "tmp/libpq.h"
#include "utils/geo-decls.h"

#include "xposthelp.h"

/**********************************************************
* Getting POSTGRES Table Info.                            *
**********************************************************/

int GetDBases(user_name)
     char *user_name;
{
  PortalBuffer *portalbuf;
  char         *res, str[200];
  int          ntups, tupno, ret_status;

  /* Get user name */
  ret_status = FindUser(user_name, &user_num);

  if (ret_status != NO_ERROR)
    return(ret_status);

  /* Open up database */
  PQsetdb("postgres");
  
  /* Start up processing */
  PQexec("begin");
  
  /* Define query for this table */
  sprintf(str, 
	  "retrieve iportal junk (p.datname) from p in pg_database where p.datdba = \"%d\" sort by datname", 
	  user_num);

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
  
  dbs_num = ntups;

  if (dbs_num > MAX_DBS)
    return(NO_DBS);
  
  for (tupno = 0; tupno < ntups; tupno++) 
    strcpy(dbs[tupno], PQgetvalue(portalbuf,tupno,0));
  
  PQexec("end");
  
  PQfinish();
  
  return(NO_ERROR);

 exit_error:
  PQexec("end");
  PQfinish();
  exit(1);
}




/*************************************************
 *                                               *
 *  Given the user name it returns the user oid. *
 *                                               *
*************************************************/

int FindUser(name, oid)
     char *name;
     int  *oid;
{
  PortalBuffer *portalbuf;
  char         *res, str[200];
  int          ntups, ret_status;
  
  /* Open up database */
  PQsetdb("postgres");
  
  /* Start up processing */
  PQexec("begin");
  
  /* Define query for this table */
  sprintf(str,"retrieve portal junk (p.usesysid) from p in pg_user where p.usename = \"%s\"", name);

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
      *oid = atoi(PQgetvalue(portalbuf,0,0));
      ret_status = NO_ERROR;
    }
  else
    {
      /* User does not exist */
      ret_status = NO_USER;
    }

  PQexec("end");
  PQfinish();
  return(ret_status);

 exit_error:
  PQexec("end");
  PQfinish();
  exit(1);

}



/*******************************
 *                             *
 *  Vacuum the given database. *
 *                             *
*******************************/

VacuumDBase(dbase)
     char *dbase;
{
  PortalBuffer *portalbuf;
  
  /* Open up database */
  PQsetdb(dbase);
  
  /* Start up processing */
  PQexec("begin");

  /* Vacuum the database */
  PQexec("vacuum");
  
  PQexec("end");
  PQfinish();

}




/********************************************
 *                                          *
 *   Get all tables for the given database. *
 *                                          *
********************************************/

GetTables(dbase)
     char *dbase;
{
  PortalBuffer *portalbuf;
  char         *res, str[200], attr_name[80];
  int          tupno, ntups, i;
  
  /* Open up database */
  PQsetdb(dbase);
  
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
  
  /* Get number of tuples in relation */
  ntups = PQntuplesGroup(portalbuf, 0);
  
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
  
  PQexec("end");
  PQfinish();
  return;

 exit_error:
  PQexec("end");
  PQfinish();
  exit(1);

}





/*********************************************
 *                                           *
 *   Get all attributes for the given table. *
 *                                           *
*********************************************/

GetAttributes(tableoid)
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

  att_num = ntups;

  /* Get the attribute name for this group and field number */
  for (i = 0; i < ntups; i++)
    {
      int *tmp;

      strcpy(att[i].name, PQgetvalue(portalbuf,i,0));
      tmp = (int *) PQgetvalue(portalbuf,i,1);
      att[i].type = *tmp;

      switch(*tmp)
	{
	case 16:
	  strcpy(att[i].str_type, "boolean");
	  break;
	  
	case 18:
	  strcpy(att[i].str_type, "char");
	  break;
	  
	case 19:
	  strcpy(att[i].str_type, "char16");
	  break;
	  
	case 22:
	  strcpy(att[i].str_type, "array");
	  break;
	  
	case 23:
	  strcpy(att[i].str_type, "int4");
	  break;
	  
	case 25:
	  strcpy(att[i].str_type, "text");
	  break;
	  
	case 26:
	  strcpy(att[i].str_type, "oid");
	  break;
	  
	case 600:
	  strcpy(att[i].str_type, "point");
	  break;
	  
	case 601:
	  strcpy(att[i].str_type, "lseg");
	  break;
	  
	case 602:
	  strcpy(att[i].str_type, "path");
	  break;
	  
	case 603:
	  strcpy(att[i].str_type, "box");
	  break;
	  
	case 604:
	  strcpy(att[i].str_type, "polygon");
	  break;
	  
	case 700:
	  strcpy(att[i].str_type, "float4");
	  break;
	  
	case 701:
	  strcpy(att[i].str_type, "point");
	  break;
	  
	default:
	  strcpy(att[i].str_type, "other");
	  break;
	  
	}/* end switch */
      
    }/* end for loop on fldno */

  PQexec("end");
  PQfinish();
  return;

 exit_error:
  PQexec("end");
  PQfinish();
  exit(1);


}



/*********************************************
 *                                           *
 *   Get all index keys for the given index. *
 *                                           *
*********************************************/

GetIndexInfo(indexoid, index_on, ind_keys)
     char *indexoid,
          *index_on,
          *ind_keys;
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
      strcpy(index_on, relname);

      strcpy(ind_keys, "");
      for (i=0; i< numkeys; i++)
	{
	  strcat(ind_keys, keys[i].attname);
	  strcat(ind_keys, "  ");
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



/************************************************
 *                                              *
 *   Get the index relation on the given table. *
 *                                              *
************************************************/

char *GetIndex(tableoid)
     char *tableoid;
{

  PortalBuffer *portalbuf;
  char         *res, str[200], index_name[80];
  
  /* Start up processing */
  PQexec("begin");
  
  /* Define query for this table */
  sprintf(str, "retrieve iportal junk (p.relname) from p in pg_class, q in pg_index where q.indrelid = \"%s\" and q.indexrelid = p.oid", tableoid);

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
  
  strcpy(index_name, PQgetvalue(portalbuf,0,0));


  PQexec("end");
  PQfinish();

  return(index_name);

 exit_error:
  PQexec("end");
  PQfinish();
  exit(1);


}
