#ifndef PIKO_OBJECT_H
#define PIKO_OBJECT_H

#include "common.h"
#include "value.h"

typedef enum {
	OBJ_STRING,
	OBJ_NATIVE,
} ObjType;

typedef struct Obj {
	ObjType type;
} Obj;

typedef struct {
	Obj obj;
	int length;
	char *chars;
} ObjString;

typedef struct {
	Obj obj;
	const char *name;
	int arity;
	bool (*function)(int arg_count, Value *args, Value *out_result);
} ObjNative;

#define OBJ_TYPE(v) (((Obj *)(v))->type)

ObjString *obj_string_copy(const char *chars, int length);
ObjNative *obj_native_new(const char *name, int arity, bool (*function)(int arg_count, Value *args, Value *out_result));
void obj_print(Obj *obj);
void obj_free(Obj *obj);

#endif
