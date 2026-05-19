#ifndef PIKO_OBJECT_H
#define PIKO_OBJECT_H

#include "common.h"
#include "chunk.h"
#include "value.h"

typedef enum {
    OBJ_STRING,
    OBJ_FUNCTION,
    OBJ_NATIVE,
    OBJ_ARRAY,
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
    char *name;
    int arity;
    Chunk chunk;
} ObjFunction;

typedef struct {
    Obj obj;
    const char *name;
    int arity;
    bool (*function)(int arg_count, Value *args, Value *out_result);
} ObjNative;

typedef struct {
    Obj obj;
    int count;
    Value *items;
} ObjArray;

#define OBJ_TYPE(v) (((Obj *)(v))->type)

ObjString *obj_string_copy(const char *chars, int length);
ObjFunction *obj_function_new(const char *name, int arity);
ObjNative *obj_native_new(const char *name, int arity, bool (*function)(int arg_count, Value *args, Value *out_result));
ObjArray *obj_array_new(Value *items, int count);
void obj_print(Obj *obj);
void obj_free(Obj *obj);

#endif
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
