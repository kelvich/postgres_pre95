/* $Header$ */

#define INNER 65000
#define OUTER 65001

/*
 *      1. index key
 *              one of:
 *                      attnum
 *                      (attnum arrayindex)
 *      2. path key
 *              (subkey1 ... subkeyN)
 *                      where subkeyI is a var node
 *              note that the 'Keys field is a list of these
 *      3. join key
 *              (outer-subkey inner-subkey)
 *                      where each subkey is a var node
 *      4. sort key
 *              one of:
 *                      SortKey node
 *                      number
 *                      nil
 *              (may also refer to the 'SortKey field of a SortKey node,
 *               which looks exactly like an index key)
 *
 */

extern bool match_indexkey_operand ARGS((LispValue indexkey, Var operand, Rel rel));
extern bool equal_indexkey_var ARGS((LispValue index_key, Var var));
extern LispValue extract_subkey ARGS((JoinKey joinKey, int which_subkey));
extern bool samekeys ARGS((LispValue keys1, LispValue keys2));
extern LispValue collect_index_pathkeys ARGS((LispValue index_keys, LispValue tlist));
extern bool match_sortkeys_pathkeys ARGS((LispValue relid, LispValue sortkeys, LispValue pathkeys));
extern bool equal_sortkey_pathkey ARGS((LispValue relid, LispValue sortkey, LispValue pathkey));
extern bool valid_sortkeys ARGS((LispValue node));
extern bool valid_numkeys ARGS((LispValue sortkeys));
