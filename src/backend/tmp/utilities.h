extern LispValue vectori_long_to_list ARGS((int *vectori, int start, int n));
extern LispValue collect ARGS((int pred, LispValue list));
/* extern LispValue same ARGS((LispValue list1, LispValue list2)); */
extern LispValue last_element ARGS((LispValue list));
extern LispValue copy_seq_tree ARGS((LispValue seqtree));
extern base_log ARGS((double foo));
extern max ARGS((int foo, int bar));
