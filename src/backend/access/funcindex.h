/* $Header$ */

#ifndef _FUNC_INDEX_INCLUDED_
#define _FUNC_INDEX_INCLUDED_

#include "tmp/postgres.h"

typedef struct {
	int		nargs;
	ObjectId	arglist[8];
	oid		procOid;
	NameData	funcName;
} FuncIndexInfo;

typedef FuncIndexInfo	*FuncIndexInfoPtr;

/*
 * some marginally useful macro definitions
 */
#define FIgetname(FINFO) (&((FINFO)->funcName.data[0]))
#define FIgetnArgs(FINFO) (FINFO)->nargs
#define FIgetProcOid(FINFO) (FINFO)->procOid
#define FIgetArg(FINFO, argnum) (FINFO)->arglist[argnum]
#define FIgetArglist(FINFO) (FINFO)->arglist

#define FIsetname(FINFO,name) strncpy(&((FINFO)->funcName.data[0]), name, 16)
#define FIsetnArgs(FINFO, numargs) ((FINFO)->nargs = numargs)
#define FIsetProcOid(FINFO, id) ((FINFO)->procOid = id)
#define FIsetArg(FINFO, argnum, argtype) ((FINFO)->arglist[argnum] = argtype)

#define FIisFunctionalIndex(FINFO) (FINFO->procOid != InvalidObjectId)

#define FIcopyFuncInfo(TO, FROM) \
	TO->nargs = FROM->nargs; \
	TO->procOid = FROM->procOid; \
	strncpy(&((TO)->funcName.data[0]), &((FROM)->funcName.data[0]), 16)

#endif _FUNC_INDEX_INCLUDED_
