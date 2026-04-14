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
	if (array->values != NULL) {
		for (int i = 0; i < array->count; i++) {
			if (IS_OBJ(array->values[i])) {
				obj_free(AS_OBJ(array->values[i]));
			}
		}
		free(array->values);
	}

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
