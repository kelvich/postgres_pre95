/* $Header$ */

#ifndef _FUNC_INDEX_INCLUDED_
#define _FUNC_INDEX_INCLUDED_


typedef struct {
	int		nargs;
	oid		procOid;
	NameData	funcName;
} FuncIndexInfo;

#define FIgetname(FINFO) (&((FINFO)->funcName.data[0]))
#define FIgetnArgs(FINFO) (FINFO)->nargs
#define FIgetProcOid(FINFO) (FINFO)->procOid

#endif _FUNC_INDEX_INCLUDED_
