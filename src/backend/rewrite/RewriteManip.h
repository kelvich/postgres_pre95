
/* module: /users/goh/postgres/src/parser/RewriteManip.c 
 * $File:$
 * $Author$
 * $Version:$
 */

void OffsetVarNodes ARGS(( List , int ));
void FixRangeTable ARGS(( List , List ));
void AddQualifications ARGS(( List , List , int ));
void HandleVarNodes ARGS(( List, List, int, int ));
