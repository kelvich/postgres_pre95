/*
 * oidcompos.h --
 *
 *   prototype file for the oid {char16,int4} composite type functions.
 *
 *  $Header$
 */

/* oidint4.c */
OidInt4 oidint4in ARGS((char *o ));
char *oidint4out ARGS((OidInt4 o ));
bool oidint4lt ARGS((OidInt4 o1 , OidInt4 o2 ));
bool oidint4le ARGS((OidInt4 o1 , OidInt4 o2 ));
bool oidint4eq ARGS((OidInt4 o1 , OidInt4 o2 ));
bool oidint4ge ARGS((OidInt4 o1 , OidInt4 o2 ));
bool oidint4gt ARGS((OidInt4 o1 , OidInt4 o2 ));
bool oidint4ne ARGS((OidInt4 o1 , OidInt4 o2 ));
bool oidint4cmp ARGS((OidInt4 o1 , OidInt4 o2 ));
OidInt4 mkoidint4 ARGS((ObjectId v_oid , uint32 v_int4 ));

/* oidint2.c */
OidInt2 oidint2in ARGS((char *o ));
char *oidint2out ARGS((OidInt2 o ));
bool oidint2lt ARGS((OidInt2 o1 , OidInt2 o2 ));
bool oidint2le ARGS((OidInt2 o1 , OidInt2 o2 ));
bool oidint2eq ARGS((OidInt2 o1 , OidInt2 o2 ));
bool oidint2ge ARGS((OidInt2 o1 , OidInt2 o2 ));
bool oidint2gt ARGS((OidInt2 o1 , OidInt2 o2 ));
bool oidint2ne ARGS((OidInt2 o1 , OidInt2 o2 ));
bool oidint2cmp ARGS((OidInt2 o1 , OidInt2 o2 ));
OidInt2 mkoidint2 ARGS((ObjectId v_oid , uint16 v_int2 ));

/* oidchar16.c */
OidChar16 oidchar16in ARGS((char *inStr ));
char *oidchar16out ARGS((OidChar16 oidname ));
bool oidchar16lt ARGS((OidChar16 o1 , OidChar16 o2 ));
bool oidchar16le ARGS((OidChar16 o1 , OidChar16 o2 ));
bool oidchar16eq ARGS((OidChar16 o1 , OidChar16 o2 ));
bool oidchar16ne ARGS((OidChar16 o1 , OidChar16 o2 ));
bool oidchar16ge ARGS((OidChar16 o1 , OidChar16 o2 ));
bool oidchar16gt ARGS((OidChar16 o1 , OidChar16 o2 ));
int oidchar16cmp ARGS((OidChar16 o1 , OidChar16 o2 ));
OidChar16 mkoidchar16 ARGS((ObjectId id , char *name ));
