#ifndef PIKO_VALUE_H
#define PIKO_VALUE_H

#include "common.h"

typedef struct Obj Obj;

typedef enum {
	VAL_NIL,
	VAL_BOOL,
	VAL_NUMBER,
	VAL_OBJ,
} ValueType;

typedef struct {
	ValueType type;
	union {
		bool boolean;
		double number;
		Obj *obj;
	} as;
} Value;

typedef struct {
	int count;
	int capacity;
	Value *values;
} ValueArray;

#define NIL_VAL ((Value){VAL_NIL, {.number = 0}})
#define BOOL_VAL(v) ((Value){VAL_BOOL, {.boolean = (v)}})
#define NUMBER_VAL(v) ((Value){VAL_NUMBER, {.number = (v)}})
#define OBJ_VAL(v) ((Value){VAL_OBJ, {.obj = (Obj *)(v)}})

#define IS_NIL(v) ((v).type == VAL_NIL)
#define IS_BOOL(v) ((v).type == VAL_BOOL)
#define IS_NUMBER(v) ((v).type == VAL_NUMBER)
#define IS_OBJ(v) ((v).type == VAL_OBJ)

#define AS_BOOL(v) ((v).as.boolean)
#define AS_NUMBER(v) ((v).as.number)
#define AS_OBJ(v) ((v).as.obj)

void value_array_init(ValueArray *array);
void value_array_write(ValueArray *array, Value value);
void value_array_free(ValueArray *array);

void print_value(Value value);
bool values_equal(Value a, Value b);

#endif
