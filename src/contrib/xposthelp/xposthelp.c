/*
 * $Header$
 */

#include <stdio.h>
#include <stdlib.h>

#include <xview/xview.h>
#include <xview/panel.h>
#include <xview/screen.h>

# include <X11/cursorfont.h>

#include "xposthelp.h"

/* Visual objects */
Display   *dpy;
Xv_Screen  screen;
int        screen_num;
Cursor     cursor;

static short icon_image[] = {
# include  "xposthelp.icon"
};

mpr_static(icon_pixrect, 64, 64, 1, icon_image);

/* Variables */
static Frame frame;
static Panel panel;
static Panel_item list_dbases, list_tables, list_attrib, 
                  message_panel, user_panel, 
                  vacuum_button, user_button,
                  show_table_msg,   show_table_panel,
                  show_indexed_msg, show_indexed_panel,
                  show_kind_msg,    show_kind_panel,
                  show_records_msg, show_records_panel,
                  show_index_msg,   show_index_panel,
                  show_keys_msg,    show_keys_panel,
                  show_attrib_msg,  show_attrib_panel;

/* Routines */
static void setup_list ();
static void create_frame();

/* Call-backs */
static int exit_proc ();
static void dbases_proc ();
static void tables_proc ();
static void attrib_proc ();
static void vacuum_proc ();
static void user_proc ();

/*************************************************************************
 *                                                                       *
 ************************************************************************/

static void create_frame()
{
  Icon my_icon;
  int  curr_row;

  my_icon = icon_create(ICON_IMAGE, &icon_pixrect, 0);
  
  frame = (Frame) xv_create (NULL, FRAME,
			     FRAME_SHOW_RESIZE_CORNER,  FALSE,
			     FRAME_ICON,                my_icon,
			     FRAME_LABEL,               "xposthelp", 
			     NULL);

  dpy        = (Display *) xv_get(frame, XV_DISPLAY);
  screen     = (Xv_Screen) xv_get(frame, XV_SCREEN);
  screen_num = (int) xv_get(screen, SCREEN_NUMBER);

  panel = (Panel) xv_create (frame, PANEL, NULL);
  
  message_panel = (Panel_item) xv_create (panel,              PANEL_MESSAGE,
					  PANEL_LABEL_STRING, NULL_STR,
					  PANEL_LABEL_BOLD,   TRUE,
					  XV_X,               10,
					  XV_Y,               10,
					  NULL);
  
  user_panel = (Panel_item) xv_create (panel,                      PANEL_TEXT,
				       PANEL_LABEL_STRING,         "User:",
				       PANEL_VALUE_DISPLAY_LENGTH, 20,
				       PANEL_VALUE_STORED_LENGTH,  80,
				       PANEL_NEXT_ROW,             25,
				       PANEL_VALUE,                NULL_STR,
				       NULL);
  
  list_dbases = (Panel_item) xv_create (panel,                   PANEL_LIST,
					PANEL_LABEL_STRING,      "Databases:",
					PANEL_LIST_DISPLAY_ROWS, 17,
					PANEL_CHOOSE_ONE,        TRUE,
					PANEL_CHOOSE_NONE,       TRUE,
					PANEL_READ_ONLY,         TRUE,
					PANEL_NOTIFY_PROC,       dbases_proc,
					PANEL_LIST_WIDTH,        180,
					PANEL_LAYOUT,            PANEL_VERTICAL,
					PANEL_NEXT_ROW,          -1,
					NULL);
  
  list_tables = (Panel_item) xv_create (panel,                   PANEL_LIST,
					PANEL_LABEL_STRING,      "Tables:",
					PANEL_LIST_DISPLAY_ROWS, 20,
					PANEL_CHOOSE_ONE,        TRUE,
					PANEL_CHOOSE_NONE,       TRUE,
					PANEL_READ_ONLY,         TRUE,
					PANEL_NOTIFY_PROC,       tables_proc,
					PANEL_LIST_WIDTH,        180,
					PANEL_LAYOUT,            PANEL_VERTICAL,
					NULL);

  list_attrib = (Panel_item) xv_create (panel,                   PANEL_LIST,
					PANEL_LABEL_STRING,      "Attributes:",
					PANEL_LIST_DISPLAY_ROWS, 20,
					PANEL_CHOOSE_ONE,        TRUE,
					PANEL_CHOOSE_NONE,       TRUE,
					PANEL_READ_ONLY,         TRUE,
					PANEL_NOTIFY_PROC,       attrib_proc,
					PANEL_LIST_WIDTH,        180,
					PANEL_LAYOUT,            PANEL_VERTICAL,
					NULL);

  /*  Buttons */
  (void) xv_create (panel,              PANEL_BUTTON,
		    PANEL_LABEL_STRING, "Exit",
		    PANEL_NOTIFY_PROC,  exit_proc,
		    XV_X,               560,
		    XV_Y,               30,
		    NULL);
  
  vacuum_button = (Panel_item) xv_create (panel,              PANEL_BUTTON,
					  PANEL_LABEL_STRING, "Vacuum Database",
					  PANEL_NOTIFY_PROC,  vacuum_proc,
					  XV_X,               20,
					  XV_Y,               420,
					  NULL);
  
  user_button = (Panel_item) xv_create (panel,              PANEL_BUTTON,
					PANEL_LABEL_STRING, "Apply User",
					PANEL_NOTIFY_PROC,  user_proc,
					XV_X,               250,
					XV_Y,               30,
					NULL);

  
  /* Show table name & indexed*/
  curr_row = 470;
  show_table_msg = (Panel_item) xv_create (panel,               PANEL_MESSAGE,
					   PANEL_LABEL_STRING, "Table: ",
					   PANEL_LABEL_BOLD,   TRUE,
					   XV_X,               10,
					   XV_Y,               curr_row,
					   NULL);
  
  show_table_panel = (Panel_item) xv_create (panel, PANEL_MESSAGE,
					     XV_X, 100,
					     XV_Y, curr_row,
					     NULL);
  
  
  show_indexed_msg = (Panel_item) xv_create (panel,               PANEL_MESSAGE,
					     PANEL_LABEL_STRING, "Indexed: ",
					     PANEL_LABEL_BOLD,   TRUE,
					     XV_X,               250,
					     XV_Y,               curr_row,
					     NULL);
  
  show_indexed_panel = (Panel_item) xv_create (panel, PANEL_MESSAGE,
					       XV_X, 320,
					       XV_Y, curr_row,
					       NULL);
  

  show_index_msg = (Panel_item) xv_create (panel,               PANEL_MESSAGE,
					   PANEL_LABEL_STRING, "Index On: ",
					   PANEL_LABEL_BOLD,   TRUE,
					   XV_X,               410,
					   XV_Y,               curr_row,
					   NULL);
  
  show_index_panel = (Panel_item) xv_create (panel, PANEL_MESSAGE,
					     XV_X, 500,
					     XV_Y, curr_row,
					     NULL);
  

  /* Show records and kind*/
  curr_row += 20;
  show_records_msg = (Panel_item) xv_create (panel,               PANEL_MESSAGE,
					     PANEL_LABEL_STRING, "Records: ",
					     PANEL_LABEL_BOLD,   TRUE,
					     XV_X,               10,
					     XV_Y,               curr_row,
					     NULL);
  
  show_records_panel = (Panel_item) xv_create (panel, PANEL_MESSAGE,
					       XV_X, 100,
					       XV_Y, curr_row,
					       NULL);
  

  show_kind_msg = (Panel_item) xv_create (panel,               PANEL_MESSAGE,
					  PANEL_LABEL_STRING, "Kind: ",
					  PANEL_LABEL_BOLD,   TRUE,
					  XV_X,               250,
					  XV_Y,               curr_row,
					  NULL);
  
  show_kind_panel = (Panel_item) xv_create (panel, PANEL_MESSAGE,
					    XV_X, 320,
					    XV_Y, curr_row,
					    NULL);
  

  show_keys_msg = (Panel_item) xv_create (panel,               PANEL_MESSAGE,
					  PANEL_LABEL_STRING, "Keys: ",
					  PANEL_LABEL_BOLD,   TRUE,
					  XV_X,               410,
					  XV_Y,               curr_row,
					  NULL);
  
  show_keys_panel = (Panel_item) xv_create (panel, PANEL_MESSAGE,
					    XV_X, 500,
					    XV_Y, curr_row,
					    NULL);
  

  /* Show Attribute type */
  curr_row += 20;
  show_attrib_msg = (Panel_item) xv_create (panel,              PANEL_MESSAGE,
					    PANEL_LABEL_STRING, "Attribute Type: ",
					    PANEL_LABEL_BOLD,   TRUE,
					    XV_X,               10,
					    XV_Y,               curr_row,
					    NULL);
  
  show_attrib_panel = (Panel_item) xv_create (panel, PANEL_MESSAGE,
					      XV_X,  130,
					      XV_Y,  curr_row,
					      NULL);
  

  window_fit (panel);
  window_fit (frame);
  
  xv_set(frame, XV_SHOW, TRUE, NULL);
}



/**********************************************
 *                                            *
 * Handle the selection of an attribute entry *
 * in the scroll list.                        *
 *                                            *
 *********************************************/


static void dbases_proc  (item, string, client_data, op, event)
     Panel_item item;
     char *string;
     caddr_t client_data;
     Panel_list_op op;
     Event *event;
{
  int i;
  /************************************************
   *                                              *
   * Make sure the user selected and didn't drag. *
   * Dragging may cause unwanted changes.         *
   *                                              *
   ***********************************************/
  
  if (op == SELECT && event_id (event) != LOC_DRAG)
    {
      /* Show clock cursor on main window  */
      cursor = XCreateFontCursor(dpy, XC_watch);
      XDefineCursor(dpy, (Window)xv_get(panel, XV_XID), cursor);
      XFlush(dpy);

      /*******************************************************
       * Hide the scroll lists while re-computing the lists. *
       * Saves on re-draw time.                              *
       ******************************************************/
      xv_set (list_tables,
	      XV_SHOW, FALSE,
	      NULL);
  
      /* First delete old entries */
      for (i = tbs_num - 1; i >= 0; i--)
	xv_set (list_tables, PANEL_LIST_DELETE, i, NULL);
  
      /* Show vacuum button */
      xv_set (vacuum_button,
	      XV_SHOW, TRUE,
	      NULL);
      
      /* reset global variable */
      strcpy(curr_dbase, string);
      
      /* Use postgres to get all tables */
      GetTables(curr_dbase);
      
      /* Put tables in scrolling list */ 
      for (i=0; i< tbs_num; i++)
	{
	  xv_set (list_tables,
		  PANEL_LIST_INSERT, i,
		  PANEL_LIST_STRING, i, tbs[i].name,
		  NULL);
	  
	}

      xv_set (list_tables,
	      XV_SHOW, TRUE,
	      NULL);
  
      /* Change cursor back to normal on main window */
      cursor = XCreateFontCursor(dpy, XC_crosshair);
      XDefineCursor(dpy, (Window)xv_get(panel, XV_XID), cursor);
      XFlush(dpy);

      
    } 
  else
    {
      /* Show vacuum button */
      xv_set (vacuum_button,
	      XV_SHOW, FALSE,
	      NULL);
      
      /* reset global variable */
      strcpy(curr_dbase, "");
      
      /* Clean table list */
      xv_set (list_tables,
	      XV_SHOW, FALSE,
	      NULL);

      /* Delete entries */
      for (i = tbs_num - 1; i >= 0; i--)
	xv_set (list_tables, PANEL_LIST_DELETE, i, NULL);

      tbs_num = 0;

      xv_set (list_tables,
	      XV_SHOW, TRUE,
	      NULL);

      /* Clean attributes list */
      xv_set (list_attrib,
	      XV_SHOW, FALSE,
	      NULL);

      /* First delete old entries */
      for (i = att_num - 1; i >= 0; i--)
	xv_set (list_attrib, PANEL_LIST_DELETE, i, NULL);

      att_num = 0;

      xv_set (list_attrib,
	      XV_SHOW, TRUE,
	      NULL);

      /* Hide panel items */
      xv_set (show_table_msg,
	      XV_SHOW, FALSE,
	      NULL);
      
      xv_set (show_table_panel,
	      XV_SHOW, FALSE,
	      NULL);
      
      xv_set (show_indexed_msg,
	      XV_SHOW, FALSE,
	      NULL);
      
      xv_set (show_indexed_panel,
	      XV_SHOW, FALSE,
	      NULL);
      
      xv_set (show_records_msg,
	      XV_SHOW, FALSE,
	      NULL);
      
      xv_set (show_records_panel,
	      XV_SHOW, FALSE,
	      NULL);
      
      xv_set (show_kind_msg,
	      XV_SHOW, FALSE,
	      NULL);
      
      xv_set (show_kind_panel,
	      XV_SHOW, FALSE,
	      NULL);
      
      xv_set (show_index_msg,
	      XV_SHOW, FALSE,
	      NULL);
      
      xv_set (show_index_panel,
	      XV_SHOW, FALSE,
	      NULL);
      
      xv_set (show_keys_msg,
	      XV_SHOW, FALSE,
	      NULL);
      
      xv_set (show_keys_panel,
	      XV_SHOW, FALSE,
	      NULL);
      
      xv_set (show_attrib_msg,
	      XV_SHOW, FALSE,
	      NULL);
      
      xv_set (show_attrib_panel,
	      XV_SHOW, FALSE,
	      NULL);
    }
}



/**********************************************
 *                                            *
 * Handle the selection of an attribute entry *
 * in the scroll list.                        *
 *                                            *
 *********************************************/

static void tables_proc  (item, string, client_data, op, event)
     Panel_item item;
     char *string;
     caddr_t client_data;
     Panel_list_op op;
     Event *event;
{

  int i, selected;
  char str[20];

  /************************************************
   *                                              *
   * Make sure the user selected and didn't drag. *
   * Dragging may cause unwanted changes.         *
   *                                              *
   ***********************************************/
  
  if (op == SELECT && event_id (event) != LOC_DRAG)
    {
      /* Show clock cursor on main window  */
      cursor = XCreateFontCursor(dpy, XC_watch);
      XDefineCursor(dpy, (Window)xv_get(panel, XV_XID), cursor);
      XFlush(dpy);

      /*******************************************************
       * Hide the scroll lists while re-computing the lists. *
       * Saves on re-draw time.                              *
       ******************************************************/
      xv_set (list_attrib,
	      XV_SHOW, FALSE,
	      NULL);
  
      /* First delete old entries */
      for (i = att_num - 1; i >= 0; i--)
	xv_set (list_attrib, PANEL_LIST_DELETE, i, NULL);

      att_num = 0;

      selected = (int) xv_get(list_tables, PANEL_LIST_FIRST_SELECTED);

      /* Use postgres to get all attributes */
      GetAttributes(tbs[selected].oid);
      
      /* Put attributes in scrolling list */ 
      for (i=0; i< att_num; i++)
	{
	  xv_set (list_attrib,
		  PANEL_LIST_INSERT, i,
		  PANEL_LIST_STRING, i, att[i].name,
		  NULL);
	  
	}

      xv_set (list_attrib,
	      XV_SHOW, TRUE,
	      NULL);
  
      xv_set (show_table_msg,
	      XV_SHOW, TRUE,
	      NULL);
      
      xv_set (show_table_panel,
	      XV_SHOW, TRUE,
	      PANEL_LABEL_STRING, string,
	      NULL);

      xv_set (show_indexed_msg,
	      XV_SHOW, TRUE,
	      NULL);

      if (tbs[selected].ind)
	{
	  char index_name[80];

	  sprintf(index_name, "yes (%s)", GetIndex(tbs[selected].oid));

	  xv_set (show_indexed_panel,
		  XV_SHOW, TRUE,
		  PANEL_LABEL_STRING, index_name,
		  NULL);
      
	}
      else
	  xv_set (show_indexed_panel,
		  XV_SHOW, TRUE,
		  PANEL_LABEL_STRING, "no",
		  NULL);
      
	
      xv_set (show_records_msg,
	      XV_SHOW, TRUE,
	      NULL);
      
      sprintf(str, "%d", tbs[selected].numrec);
      xv_set (show_records_panel,
	      XV_SHOW, TRUE,
	      PANEL_LABEL_STRING, str,
	      NULL);
      
      xv_set (show_kind_msg,
	      XV_SHOW, TRUE,
	      NULL);
      
      xv_set (show_kind_panel,
	      XV_SHOW, TRUE,
	      PANEL_LABEL_STRING, (tbs[selected].kind) ? "relation": "index",
	      NULL);

      /* Show only if table is an index */
      if (!tbs[selected].kind)
	{
	  char index_on[80], ind_keys[200];

	  GetIndexInfo(tbs[selected].oid, index_on, ind_keys);

	  xv_set (show_index_msg,
		  XV_SHOW, TRUE,
		  NULL);
	  
	  xv_set (show_index_panel,
		  XV_SHOW, TRUE,
		  PANEL_LABEL_STRING, index_on,
		  NULL);
	  
	  xv_set (show_keys_msg,
		  XV_SHOW, TRUE,
		  NULL);
	  
	  xv_set (show_keys_panel,
		  XV_SHOW, TRUE,
		  PANEL_LABEL_STRING, ind_keys,
		  NULL);
	}

      /* Change cursor back to normal on main window */
      cursor = XCreateFontCursor(dpy, XC_crosshair);
      XDefineCursor(dpy, (Window)xv_get(panel, XV_XID), cursor);
      XFlush(dpy);

      
    } 
  else
    {
      xv_set (list_attrib,
	      XV_SHOW, FALSE,
	      NULL);

      /* First delete old entries */
      for (i = att_num - 1; i >= 0; i--)
	xv_set (list_attrib, PANEL_LIST_DELETE, i, NULL);

      att_num = 0;

      xv_set (list_attrib,
	      XV_SHOW, TRUE,
	      NULL);

      xv_set (show_table_msg,
	      XV_SHOW, FALSE,
	      NULL);
      
      xv_set (show_table_panel,
	      XV_SHOW, FALSE,
	      NULL);
      
      xv_set (show_indexed_msg,
	      XV_SHOW, FALSE,
	      NULL);
      
      xv_set (show_indexed_panel,
	      XV_SHOW, FALSE,
	      NULL);
      
      xv_set (show_records_msg,
	      XV_SHOW, FALSE,
	      NULL);
      
      xv_set (show_records_panel,
	      XV_SHOW, FALSE,
	      NULL);
      
      xv_set (show_kind_msg,
	      XV_SHOW, FALSE,
	      NULL);
      
      xv_set (show_kind_panel,
	      XV_SHOW, FALSE,
	      NULL);
      
      xv_set (show_index_msg,
	      XV_SHOW, FALSE,
	      NULL);
      
      xv_set (show_index_panel,
	      XV_SHOW, FALSE,
	      NULL);
      
      xv_set (show_keys_msg,
	      XV_SHOW, FALSE,
	      NULL);
      
      xv_set (show_keys_panel,
	      XV_SHOW, FALSE,
	      NULL);
      
      xv_set (show_attrib_msg,
	      XV_SHOW, FALSE,
	      NULL);
      
      xv_set (show_attrib_panel,
	      XV_SHOW, FALSE,
	      NULL);
    }

} 



/**********************************************
 *                                            *
 * Handle the selection of an attribute entry *
 * in the scroll list.                        *
 *                                            *
 *********************************************/

static void attrib_proc  (item, string, client_data, op, event)
     Panel_item item;
     char *string;
     caddr_t client_data;
     Panel_list_op op;
     Event *event;
{
  int i;
  char str[100];

  if (op == SELECT && event_id (event) != LOC_DRAG)
    {
      xv_set (show_attrib_msg,
	      XV_SHOW, TRUE,
	      NULL);
      
      i = (int) xv_get(list_attrib, PANEL_LIST_FIRST_SELECTED);

      sprintf(str, " %s (%d)", att[i].str_type, att[i].type);

      xv_set (show_attrib_panel,
	      XV_SHOW, TRUE,
	      PANEL_LABEL_STRING, str,
	      NULL);

    }
  else
    {
      xv_set (show_attrib_msg,
	      XV_SHOW, FALSE,
	      NULL);
      
      xv_set (show_attrib_panel,
	      XV_SHOW, FALSE,
	      NULL);
    }

} 



/******************************************
 *                                        *
 * Setup initial list for the given user. *
 *                                        *
 *****************************************/

static void setup_list ()
{

  int i, ret_status;

  /* Show clock cursor on main window  */
  cursor = XCreateFontCursor(dpy, XC_watch);
  XDefineCursor(dpy, (Window)xv_get(panel, XV_XID), cursor);
  XFlush(dpy);
  
  /* Hide items that are supposed to show */
  xv_set (vacuum_button,
	  XV_SHOW, FALSE,
	  NULL);

  xv_set (show_table_msg,
	  XV_SHOW, FALSE,
	  NULL);

  xv_set (show_table_panel,
	  XV_SHOW, FALSE,
	  NULL);

  xv_set (show_indexed_msg,
	  XV_SHOW, FALSE,
	  NULL);
  
  xv_set (show_indexed_panel,
	  XV_SHOW, FALSE,
	  NULL);
  
  xv_set (show_records_msg,
	  XV_SHOW, FALSE,
	  NULL);
  
  xv_set (show_records_panel,
	  XV_SHOW, FALSE,
	  NULL);
  
  xv_set (show_kind_msg,
	  XV_SHOW, FALSE,
	  NULL);
  
  xv_set (show_kind_panel,
	  XV_SHOW, FALSE,
	  NULL);
  
  xv_set (show_index_msg,
	  XV_SHOW, FALSE,
	  NULL);
  
  xv_set (show_index_panel,
	  XV_SHOW, FALSE,
	  NULL);
  
  xv_set (show_keys_msg,
	  XV_SHOW, FALSE,
	  NULL);
  
  xv_set (show_keys_panel,
	  XV_SHOW, FALSE,
	  NULL);
  
  xv_set (show_attrib_msg,
	  XV_SHOW, FALSE,
	  NULL);

  xv_set (show_attrib_panel,
	  XV_SHOW, FALSE,
	  NULL);

  /*******************************************************
   * Hide the scroll lists while re-computing the lists. *
   * Saves on re-draw time.                              *
   ******************************************************/
  
  xv_set (list_dbases,
	  XV_SHOW, FALSE,
	  NULL);
  
  xv_set (list_tables,
	  XV_SHOW, FALSE,
	  NULL);
  
  xv_set (list_attrib,
	  XV_SHOW, FALSE,
	  NULL);
  
  /*
   * First delete the old entries
   */
  
  for (i = dbs_num - 1; i >= 0; i--)
    xv_set (list_dbases, PANEL_LIST_DELETE, i, NULL);
  
  for (i = tbs_num - 1; i >= 0; i--)
    xv_set (list_tables, PANEL_LIST_DELETE, i, NULL);
  
  for (i = att_num - 1; i >= 0; i--)
    xv_set (list_attrib, PANEL_LIST_DELETE, i, NULL);
  
  dbs_num = 0;
  tbs_num = 0;
  att_num = 0;

  /*
   * Now read the new entries
   */
  if (strcmp (NULL_STR, (char *) xv_get (user_panel, PANEL_VALUE))) 
    {
      ret_status = GetDBases(xv_get (user_panel, PANEL_VALUE));

      if (ret_status == NO_ERROR)
	{
	  /* Fill up the scrolling list */
	  for (i=0; i < dbs_num; i++) 
	    {
	      xv_set (list_dbases,
		      PANEL_LIST_INSERT, i,
		      PANEL_LIST_STRING, i, dbs[i],
		      NULL);
	      
	    }

	  xv_set (message_panel,
		  PANEL_LABEL_STRING,  NULL_STR,
		  NULL);

	}
      else
	ErrorHandle(ret_status);
    }

  /*
   * Done, show the new scroll panels
   */
  
  xv_set (list_dbases,
	  XV_SHOW, TRUE,
	  NULL);
  
  xv_set (list_tables,
	  XV_SHOW, TRUE,
	  NULL);
  
  xv_set (list_attrib,
	  XV_SHOW, TRUE,
	  NULL);
  
  /* Change cursor back to normal on main window */
  cursor = XCreateFontCursor(dpy, XC_crosshair);
  XDefineCursor(dpy, (Window)xv_get(panel, XV_XID), cursor);
  XFlush(dpy);
  
}



/****************************
 *                          *
 * Vacuum button call back  *
 *                          *
 ***************************/

static void vacuum_proc (item, event)
     Panel_item item;
     Event *event;
{
  /* User wants to vacuum a database */
  VacuumDBase(curr_dbase);

}



/*************************
 *                       *
 * User button call back *
 *                       *
 ************************/

static void user_proc (item, event)
     Panel_item item;
     Event *event;
{
  /* User wants to take a look at another user */
  setup_list();

}



/*************************************************************************
 *                                                                       *
 * Exit                                                                  *
 *                                                                       *
 ************************************************************************/

static int exit_proc (item, event)
     Panel_item item;
     Event *event;
     
{
  xv_set (frame, XV_SHOW, FALSE, NULL);
  xv_destroy_safe(frame);
}




/*************************************************************************
 *                                                                       *
 *  Main                                                                 *
 *                                                                       *
 ************************************************************************/

ErrorHandle(error_id)
     int error_id;
{
  char err_str[200];

  switch(error_id)
    {
    case 1:
      /* User does not exist */
      sprintf(err_str, 
	     "User %s does not exist in this system.", 
	     xv_get (user_panel, PANEL_VALUE));
      break;

    case 2:
      /* Maximum number of databases reached */
      sprintf(err_str, 
	      "The maximum number of databases (%d) has been reached.",
	      MAX_DBS);
      break;

    case 3:
      /* Maximum number of tables reached */
      sprintf(err_str, 
	      "The maximum number of tables (%d) has been reached.",
	      MAX_REL);
      break;

    }

  xv_set (message_panel,
	  PANEL_LABEL_STRING,  err_str,
	  NULL);

}

      


/*************************************************************************
 *                                                                       *
 *  Main                                                                 *
 *                                                                       *
 ************************************************************************/

main(argc, argv)
     int argc;
     char *argv[];
{
  char str[80], *text;

  /* Initialize server */
  xv_init(XV_INIT_ARGS, argc, argv, NULL);

  /* Show current directory/files combo */
  create_frame();

  /* Let the USER be the default. */
  xv_set (user_panel,
	  PANEL_VALUE, getenv("USER"),
	  NULL);

  setup_list();

  xv_main_loop(frame);
}   


