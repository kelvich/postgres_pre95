/* $Header$ */

extern Name AttributeGetAttName ARGS((Attribute attribute));
extern bool op_class ARGS((LispValue opid, LispValue opclass));
extern Name get_attname ARGS((ObjectId relid, AttributeNumber attnum));
extern AttributeNumber get_attnum ARGS((ObjectId relid, Name attname));
extern ObjectId get_atttype ARGS((ObjectId relid, AttributeNumber attnum));
extern RegProcedure get_opcode ARGS((ObjectId opid));
extern LispValue op_mergesortable ARGS((ObjectId opid, ObjectId ltype, ObjectId rtype));
extern ObjectId op_hashjoinable ARGS((ObjectId opid, ObjectId ltype, ObjectId rtype));
extern ObjectId get_commutator ARGS((ObjectId opid));
extern ObjectId get_negator ARGS((ObjectId opid));
extern RegProcedure get_oprrest ARGS((ObjectId opid));
extern RegProcedure get_oprjoin ARGS((ObjectId opid));
extern ObjectId get_regproc ARGS((Name funname));
extern AttributeNumber get_relnatts ARGS((ObjectId relid));
extern Name get_rel_name ARGS((ObjectId relid));
extern struct varlena * get_relstub ARGS((ObjectId relid));
extern ObjectId get_ruleid ARGS((Name rulename));
extern ObjectId get_eventrelid ARGS((ObjectId ruleid));
extern int16 get_typlen ARGS((ObjectId typid));
extern bool get_typbyval ARGS((ObjectId typid));
extern struct varlena *get_typdefault ARGS((ObjectId typid));
extern char get_typtype ARGS((ObjectId typid));
