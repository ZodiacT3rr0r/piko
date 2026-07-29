#ifndef PIKO_OBJECT_H
#define PIKO_OBJECT_H

#include "common.h"
#include "chunk.h"
#include "value.h"

typedef enum {
	OBJ_STRING,
	OBJ_FUNCTION,
	OBJ_CLOSURE,
	OBJ_UPVALUE,
	OBJ_MAP,
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

typedef struct ObjClosure {
	Obj obj;
	ObjFunction *function;
	int capture_count;
	struct ObjUpvalue **captures;
} ObjClosure;

typedef struct ObjUpvalue {
	Obj obj;
	Value *location;
	Value closed;
	struct ObjUpvalue *next;
} ObjUpvalue;

typedef struct {
	ObjString *key;
	Value value;
} ObjMapEntry;

typedef struct {
	Obj obj;
	int count;
	int capacity;
	ObjMapEntry *entries;
} ObjMap;

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
ObjClosure *obj_closure_new(ObjFunction *function, int capture_count);
ObjUpvalue *obj_upvalue_new(Value *location);
ObjMap *obj_map_new(void);
bool obj_map_set(ObjMap *map, const char *key, int length, Value value);
bool obj_map_get(ObjMap *map, const char *key, int length, Value *out_value);
bool obj_map_remove(ObjMap *map, const char *key, int length);
ObjNative *obj_native_new(const char *name, int arity, bool (*function)(int arg_count, Value *args, Value *out_result));
ObjArray *obj_array_new(Value *items, int count);
void obj_print(Obj *obj);
void obj_free(Obj *obj);

#endif
