/* ----------------------------------------------------------------
 *   FILE
 *	tcopprot.h
 *
 *   DESCRIPTION
 *	prototypes for postgres.c.
 *
 *   NOTES
 *	Automatically generated using mkproto
 *
 * This file was created so that other c files could get the two
 * function prototypes without having to include tcop.h which single
 * handedly includes the whole f*cking tree -- mer 5 Nov. 1991
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#ifndef tcopprotIncluded		/* include this file only once */
#define tcopprotIncluded	1

extern char InteractiveBackend ARGS((char *inBuf));
extern char SocketBackend ARGS((char *inBuf));
extern char ReadCommand ARGS((char *inBuf));

#ifndef BOOTSTRAP_INCLUDE
extern List pg_plan ARGS((String query_string, ObjectId *typev, int nargs, LispValue *parsetreeP, CommandDest dest));
extern void pg_eval ARGS((String query_string, char *argv, ObjectId *typev, int nargs));
extern void pg_eval_dest ARGS((String query_string, char *argv, ObjectId *typev, int nargs, CommandDest dest));
#endif /* BOOTSTRAP_HEADER */

extern void handle_warn ARGS((void));
extern void quickdie ARGS((void));
extern void die ARGS((void));
extern int PostgresMain ARGS((int argc, char *argv[]));
extern int ResetUsage ARGS((void));
extern int ShowUsage ARGS((void));

#endif /* tcopprotIncluded */
