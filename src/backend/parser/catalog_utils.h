#ifndef _P_CATALOG_UTILS_H
#define _P_CATALOG_UTILS_H \
  "$Header$"

#include "tmp/postgres.h"

#include "access/htup.h"
#include "utils/rel.h"
#include "catalog/pg_proc.h"
#include "catalog/pg_type.h"
#include "catalog/syscache.h"
    
typedef HeapTuple	Type;
typedef HeapTuple	Operator;

extern Type type(), get_id_type();
extern OID att_typeid(), typeid();
extern bool varisset();
extern int16 tlen();
extern bool tbyval();
extern Relation get_rdesc(), get_rgdesc();
extern char *outstr(), *instr(), *instr1(), *instr2();
extern Operator oper(), right_oper(), left_oper();
extern Name tname(), get_id_typname();
extern bool func_get_detail();
extern void func_error();



#endif
