/* $Header$ */
extern DebugVariablePtr SearchDebugVariables ARGS((String debug_variable));
extern bool DebugVariableSet ARGS((String debug_variable, int debug_value));
extern bool DebugVariableProcessCommand ARGS((char *buf));
extern void DebugVariableFileSet ARGS((String filename));
