/*
 * jbcomm.h : IPC constants & structs
 *
 * Copyright, supplied AS-IS, and all that.
 */
/* $Header$ */

#define JB_SERVER_PORT	5000
#define JB_SERVER_MACH	"hermes.Berkeley.EDU"

/*
 * Packet format:
 *
 * offset   description
 *	  +-----------------------------+
 * +00	  | int  message_length		|
 *	  +-----------------------------+
 * +04	  | short message_type		|
 *	  +-----------------------------+
 * +06	  | short unused		|
 *	  +-----------------------------+
 * +08	  | char[] data			|
 *	  +-----------------------------+
 *
 * All numbers are in network order.
 * All ASCII strings are NULL-terminated.  Data which follows an ASCII
 * string begins the byte after the '\0'.
 * "unused" is there to keep the data longword-aligned.  It will likely
 * be used for something eventually...
 */

#define JB_DATA_START	8		/* data starts 8 bytes in */

/*
 * Requests (message_type constants)
 */
#define JB_INIT_REQ		0	/* initialization request */
/* (no data) */
#define JB_OPEN_REQ		1	/* jb_open */
/* +00 flags [note this is backward from procedure args order] */
/* +04 platter_id (ASCII string) */
/* +xx format (ASCII string) */
#define JB_READ_REQ		2	/* jb_read */
/* +00 platter */
/* +04 start block # */
/* +08 #of blocks */
#define JB_WRITE_REQ		3	/* jb_write */
/* +00 platter */
/* +04 start block # */
/* +08 #of blocks */
/* +12 data (byte stream) */
#define JB_CLOSE_REQ		4	/* jb_close */
/* +00 platter */
#define JB_ISTAT_REQ		5	/* jb_istat */
/* +00 platter_id (ASCII string) */
#define JB_FSTAT_REQ		6	/* jb_fstat */
/* +00 platter */
#define JB_SCANB_REQ		7	/* jb_scanb */
/* +00 platter */
/* +04 start_block */
/* +08 search_max */
#define JB_SCANW_REQ		8	/* jb_scanw */
/* +00 platter */
/* +04 start_block */
/* +08 search_max */
#define JB_EJECT_REQ		9	/* jb_eject */
/* +00 platter_id (ASCII string) */
#define JB_LOAD_REQ		10	/* jb_load */
/* (no data) */
#define JB_LABEL_REQ		11	/* jb_label */
/* +00 platter_id (ASCII string) */
/* +xx label (1024 byte label block) */
#define JB_EXIT_REQ		12	/* jb_exit */
/* (no data) */

#define JB_OFFSET_REQ		13	/* jb_offset */
/* +00 platter */
/* +04 offset */
#define JB_SHELF_OFFSET_REQ	14	/* jb_shelf_offset */
/* +00 shelf */
/* +04 side */
/* +08 offset */

/*
 * Replies
 *
 * If "error status" is nonzero, the rest of the fields are invalid (and
 * may not exist).  All REPLYs must have the error status field.
 */
#define JB_INIT_REPLY		0	/* initialization reply */
/* +00 error status */
#define JB_OPEN_REPLY		1	/* jb_open */
/* +00 error status */
/* +04 platter */
#define JB_READ_REPLY		2	/* jb_read */
/* +00 error status */
/* +04 data (byte stream; use packet length) */
#define JB_WRITE_REPLY		3	/* jb_write */
/* +00 error status */
#define JB_CLOSE_REPLY		4	/* jb_close */
/* +00 error status */
#define JB_ISTAT_REPLY		5	/* jb_istat */
/* +00 error status */
/* +04 JBSTATUS */
#define JB_FSTAT_REPLY		6	/* jb_fstat */
/* +00 error status */
/* +04 JBSTATUS */
#define JB_SCANB_REPLY		7	/* jb_scanb */
/* +00 error status */
/* +04 block (-1 if none found) */
#define JB_SCANW_REPLY		8	/* jb_scanw */
/* +00 error status */
/* +04 block (-1 if none found) */
#define JB_EJECT_REPLY		9	/* jb_eject */
/* +00 error status */
#define JB_LOAD_REPLY		10	/* jb_load */
/* +00 error status */
#define JB_LABEL_REPLY		11	/* jb_label */
/* +00 error status */
#define JB_EXIT_REPLY		12	/* jb_exit */
/* +00 error status */

#define JB_OFFSET_REPLY		13	/* jb_offset */
/* +00 error status */
#define JB_SHELF_OFFSET_REPLY	14	/* jb_shelf_offset */
/* +00 error status */


/*
 * Format of JBSTATUS replies
 */
/* +00 error status */
/* +04 JBSTATUS: */
/*     +04 location */
/*     +08 access */
/*     +12 locked */
/*     +16 offset */
/*     +20 JBLABEL (1024 bytes) */
#define JBSTATUS_SIZE	4*4		/* size w/o JBLABEL or error status */

