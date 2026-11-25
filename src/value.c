#include "value.h"

#include "object.h"

static int grow_capacity(int capacity) {
	return capacity < 8 ? 8 : capacity * 2;
}

void value_array_init(ValueArray *array) {
	array->count = 0;
	array->capacity = 0;
	array->values = NULL;
}

void value_array_write(ValueArray *array, Value value) {
	if (array->count + 1 > array->capacity) {
		int old_capacity = array->capacity;
		array->capacity = grow_capacity(old_capacity);
		array->values = (Value *)realloc(array->values, sizeof(Value) * (size_t)array->capacity);
		if (array->values == NULL) {
			fprintf(stderr, "piko: out of memory\n");
			exit(1);
		}
	}

	array->values[array->count] = value;
	array->count++;
}

void value_array_free(ValueArray *array) {
	free(array->values);
	value_array_init(array);
}

void print_value(Value value) {
	switch (value.type) {
		case VAL_NIL:
			printf("nil");
			break;
		case VAL_BOOL:
			printf(AS_BOOL(value) ? "true" : "false");
			break;
		case VAL_NUMBER:
			printf("%g", AS_NUMBER(value));
			break;
		case VAL_OBJ:
			obj_print(AS_OBJ(value));
			break;
	}
}

bool values_equal(Value a, Value b) {
	if (a.type != b.type) {
		return false;
	}

	switch (a.type) {
		case VAL_NIL:
			return true;
		case VAL_BOOL:
			return AS_BOOL(a) == AS_BOOL(b);
		case VAL_NUMBER:
			return AS_NUMBER(a) == AS_NUMBER(b);
		case VAL_OBJ:
			if (AS_OBJ(a) == AS_OBJ(b)) {
				return true;
			}
			if (OBJ_TYPE(AS_OBJ(a)) == OBJ_STRING && OBJ_TYPE(AS_OBJ(b)) == OBJ_STRING) {
				ObjString *sa = (ObjString *)AS_OBJ(a);
				ObjString *sb = (ObjString *)AS_OBJ(b);
				return sa->length == sb->length &&
					   sa->hash == sb->hash &&
					   memcmp(sa->chars, sb->chars, (size_t)sa->length) == 0;
			}
			return false;
	}

	return false;
}
