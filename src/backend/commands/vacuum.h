/*
 *  vacuum.h -- header file for postgres vacuum cleaner
 *
 *	$Header$
 */

#ifndef _VACUUM_H_
#define _VACUUM_H_

typedef struct VAttListData {
    int			val_dummy;
    struct VAttListData	*val_next;
} VAttListData;

typedef VAttListData	*VAttList;

typedef struct VTidListData {
    ItemPointerData	vtl_tid;
    struct VTidListData	*vtl_next;
} VTidListData;

typedef VTidListData	*VTidList;

typedef struct VRelListData {
    ObjectId		vrl_relid;
    VAttList		vrl_attlist;
    VTidList		vrl_tidlist;
    int			vrl_ntups;
    int			vrl_npages;
    bool		vrl_hasindex;
    struct VRelListData	*vrl_next;
} VRelListData;

typedef VRelListData	*VRelList;

/* routines */
extern void	_vc_init();
extern void	_vc_shutdown();
extern void	_vc_vacuum();
extern VRelList	_vc_getrels();
extern void	_vc_vacone();
extern void	_vc_vacheap();
extern void	_vc_vacindices();
extern void	_vc_vaconeind();
extern void	_vc_updstats();
extern bool	_vc_ontidlist();
extern void	_vc_reaptid();
extern void	_vc_free();
extern void	_vc_setpagelock();
extern Relation	_vc_getarchrel();
extern void	_vc_archive();

#endif /* ndef _VACUUM_H_ */
