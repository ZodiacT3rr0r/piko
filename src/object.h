#ifndef PIKO_OBJECT_H
#define PIKO_OBJECT_H

#include "common.h"

typedef enum {
	OBJ_STRING,
} ObjType;

typedef struct Obj {
	ObjType type;
} Obj;

typedef struct {
	Obj obj;
	int length;
	char *chars;
} ObjString;

#define OBJ_TYPE(v) (((Obj *)(v))->type)

ObjString *obj_string_copy(const char *chars, int length);
void obj_print(Obj *obj);
void obj_free(Obj *obj);

#endif
