#ifndef _P_CATALOG_UTILS_H
#define _P_CATALOG_UTILS_H \
  "$Header$"

#include "c.h"
#include "cat.h"

#include "htup.h"
#include "rel.h"
#include "oid.h"

typedef struct tuple *Type;
typedef struct tuple *Operator;

extern Type type(), get_id_type();
extern OID att_typeid(), typeid();
extern Relation get_rdesc(), get_rgdesc();
extern char *outstr(), *instr(), *instr1(), *instr2();
extern Operator oper(), right_oper(), left_oper();
extern Name tname();

#endif
