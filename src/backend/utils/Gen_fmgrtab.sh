#! /bin/sh
# ----------------------------------------------------------------
#   FILE
#	Gen_fmgrtab.sh
#
#   DESCRIPTION
#
#   NOTES
#	This is a quickly hacked version because I left the real
#	version at home.  It will be replaced shortly.
#
#   IDENTIFICATION
# 	$Header$
# ----------------------------------------------------------------
PATH=$PATH:/lib:/usr/lib		# to find cpp
BKIOPTS=''
if [ $? != 0 ]
then
    echo `basename $0`: Bad option
    exit 1
fi

for opt in $*
do
    case $opt in
    -D) BKIOPTS="$BKIOPTS -D$2"; shift; shift;;
    -D*) BKIOPTS="$BKIOPTS $1";shift;;
    --) shift; break;;
    esac
done

INFILE=$1
RAWFILE=fmgrtab.raw
HFILE=fmgr.h
CFILE=fmgrtab.c

awk '
BEGIN		{ raw = 0; }
/^DATA/		{ print; next; }
/^BKI_BEGIN/ 	{ raw = 1; next; }
/^BKI_END/ 	{ raw = 0; next; }
raw == 1 	{ print; next; }' $INFILE | \
sed -e 's/^.*OID[^=]*=[^0-9]*//' \
 -e 's/[ 	]*).*$//' \
 -e 's/[ 	]*([ 	]*/ /' | \
awk '
$4 == "11"	{ print; next; }
/^#/		{ print; next; }' | \
cpp $BKIOPTS | \
egrep '^[0-9]' | \
sort -n > $RAWFILE

cat > $HFILE <<FooBar
#ifndef FmgrIncluded
#define FmgrIncluded

typedef char *  ((*func_ptr)());        /* ptr to func returning (char *) */

#include "utils/dynamic_loader.h"

extern char     *fmgr();
extern char	*fmgr_c();
extern void	fmgr_info();
extern char	*fmgr_ptr();
extern char	*fmgr_dynamic();
extern char	*fmgr_array_args();

#define FMGR_PTR2(FP, FO, A1, A2) \
(((func_ptr)FP) ? (*((func_ptr)FP))(A1, A2) : fmgr_ptr(FP, FO, 2, A1, A2))

#define MAXFMGRARGS     9

typedef struct {
	char	*data[MAXFMGRARGS];
} FmgrValues;

#define SEL_CONSTANT    1       /* constant does not vary (not a parameter) */
#define SEL_RIGHT       2       /* constant appears to right of operator */

FooBar

awk '{ print $2, $1; }' $RAWFILE | \
tr 'a-z' 'A-Z' | sed -e 's/^/#define F_/' >> $HFILE

cat >> $HFILE <<FooBar
#endif
FooBar

awk '{ print "char *" $2 "();"; }' $RAWFILE > $CFILE

cat >> $CFILE <<FooBar
#include "utils/fmgrtab.h"

static FmgrCall fmgr_builtins[] = {
FooBar

awk '{ print "{", $1 ", " $8 ", " $2 " },"; }' $RAWFILE >> $CFILE

cat >> $CFILE <<FooBar
};

static int fmgr_nbuiltins = (sizeof(fmgr_builtins)/sizeof(FmgrCall))-1;

FmgrCall *
fmgr_isbuiltin(procedureId)
	ObjectId procedureId;
{
	int i, low, high;

        low = 0;
        high = fmgr_nbuiltins;
        while (low <= high) {
                i = low + (high - low) / 2;
                if (procedureId == fmgr_builtins[i].proid)
                        break;
                else if (procedureId > fmgr_builtins[i].proid)
                        low = i + 1;
                else
                        high = i - 1;
        }
	if (procedureId == fmgr_builtins[i].proid)
		return(&fmgr_builtins[i]);
	return((FmgrCall *) NULL);
}
FooBar

rm -f $RAWFILE

exit 0
