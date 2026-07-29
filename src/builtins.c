#include "builtins.h"

#include <ctype.h>
#include <time.h>

#include "object.h"

static bool builtin_len(int arg_count, Value *args, Value *out_result) {
	if (arg_count != 1) {
		fprintf(stderr, "len expects 1 argument.\n");
		return false;
	}

	if (!IS_OBJ(args[0])) {
		fprintf(stderr, "len expects a collection or string.\n");
		return false;
	}

	switch (OBJ_TYPE(AS_OBJ(args[0]))) {
		case OBJ_STRING: {
			ObjString *string = (ObjString *)AS_OBJ(args[0]);
			*out_result = NUMBER_VAL((double)string->length);
			return true;
		}
		case OBJ_ARRAY: {
			ObjArray *array = (ObjArray *)AS_OBJ(args[0]);
			*out_result = NUMBER_VAL((double)array->count);
			return true;
		}
		case OBJ_MAP: {
			ObjMap *map = (ObjMap *)AS_OBJ(args[0]);
			*out_result = NUMBER_VAL((double)map->count);
			return true;
		}
		default:
			fprintf(stderr, "len expects a string, array, or map.\n");
			return false;
	}
}

static bool builtin_push(int arg_count, Value *args, Value *out_result) {
	if (arg_count != 2) {
		fprintf(stderr, "push expects 2 arguments.\n");
		return false;
	}

	if (!IS_OBJ(args[0]) || OBJ_TYPE(AS_OBJ(args[0])) != OBJ_ARRAY) {
		fprintf(stderr, "push expects an array.\n");
		return false;
	}

	ObjArray *array = (ObjArray *)AS_OBJ(args[0]);
	int new_count = array->count + 1;
	Value *grown = (Value *)realloc(array->items, sizeof(Value) * (size_t)new_count);
	if (grown == NULL) {
		fprintf(stderr, "piko: out of memory\n");
		exit(1);
	}

	array->items = grown;
	array->items[array->count] = args[1];
	array->count = new_count;
	*out_result = args[0];
	return true;
}

static bool builtin_pop(int arg_count, Value *args, Value *out_result) {
	if (arg_count != 1) {
		fprintf(stderr, "pop expects 1 argument.\n");
		return false;
	}

	if (!IS_OBJ(args[0]) || OBJ_TYPE(AS_OBJ(args[0])) != OBJ_ARRAY) {
		fprintf(stderr, "pop expects an array.\n");
		return false;
	}

	ObjArray *array = (ObjArray *)AS_OBJ(args[0]);
	if (array->count == 0) {
		*out_result = NIL_VAL;
		return true;
	}

	*out_result = array->items[array->count - 1];
	array->count--;
	return true;
}

static bool builtin_type(int arg_count, Value *args, Value *out_result) {
	if (arg_count != 1) {
		fprintf(stderr, "type expects 1 argument.\n");
		return false;
	}

	const char *name = "unknown";
	if (IS_NIL(args[0])) {
		name = "nil";
	} else if (IS_BOOL(args[0])) {
		name = "bool";
	} else if (IS_NUMBER(args[0])) {
		name = "number";
	} else if (IS_OBJ(args[0])) {
		switch (OBJ_TYPE(AS_OBJ(args[0]))) {
			case OBJ_STRING: name = "string"; break;
			case OBJ_FUNCTION: name = "function"; break;
			case OBJ_CLOSURE: name = "closure"; break;
			case OBJ_UPVALUE: name = "upvalue"; break;
			case OBJ_MAP: name = "map"; break;
			case OBJ_NATIVE: name = "native"; break;
			case OBJ_ARRAY: name = "array"; break;
		}
	}

	*out_result = OBJ_VAL(obj_string_copy(name, (int)strlen(name)));
	return true;
}

static bool builtin_str(int arg_count, Value *args, Value *out_result) {
	if (arg_count != 1) {
		fprintf(stderr, "str expects 1 argument.\n");
		return false;
	}

	char buffer[64];
	const char *text = NULL;
	if (IS_NIL(args[0])) {
		text = "nil";
	} else if (IS_BOOL(args[0])) {
		text = AS_BOOL(args[0]) ? "true" : "false";
	} else if (IS_NUMBER(args[0])) {
		snprintf(buffer, sizeof(buffer), "%g", AS_NUMBER(args[0]));
		text = buffer;
	} else if (IS_OBJ(args[0])) {
		switch (OBJ_TYPE(AS_OBJ(args[0]))) {
			case OBJ_STRING:
				text = ((ObjString *)AS_OBJ(args[0]))->chars;
				break;
			default:
				text = "<object>";
				break;
		}
	} else {
		text = "<value>";
	}

	*out_result = OBJ_VAL(obj_string_copy(text, (int)strlen(text)));
	return true;
}

static bool builtin_int(int arg_count, Value *args, Value *out_result) {
	if (arg_count != 1) {
		fprintf(stderr, "int expects 1 argument.\n");
		return false;
	}

	if (IS_NUMBER(args[0])) {
		*out_result = NUMBER_VAL((double)((int)AS_NUMBER(args[0])));
		return true;
	}
	if (IS_OBJ(args[0]) && OBJ_TYPE(AS_OBJ(args[0])) == OBJ_STRING) {
		ObjString *string = (ObjString *)AS_OBJ(args[0]);
		*out_result = NUMBER_VAL((double)strtol(string->chars, NULL, 10));
		return true;
	}

	fprintf(stderr, "int expects a number or numeric string.\n");
	return false;
}

static bool builtin_float(int arg_count, Value *args, Value *out_result) {
	if (arg_count != 1) {
		fprintf(stderr, "float expects 1 argument.\n");
		return false;
	}

	if (IS_NUMBER(args[0])) {
		*out_result = NUMBER_VAL(AS_NUMBER(args[0]));
		return true;
	}
	if (IS_OBJ(args[0]) && OBJ_TYPE(AS_OBJ(args[0])) == OBJ_STRING) {
		ObjString *string = (ObjString *)AS_OBJ(args[0]);
		*out_result = NUMBER_VAL(strtod(string->chars, NULL));
		return true;
	}

	fprintf(stderr, "float expects a number or numeric string.\n");
	return false;
}

static bool builtin_clock(int arg_count, Value *args, Value *out_result) {
	(void)args;
	if (arg_count != 0) {
		fprintf(stderr, "clock expects 0 arguments.\n");
		return false;
	}

	*out_result = NUMBER_VAL((double)clock() / (double)CLOCKS_PER_SEC);
	return true;
}

static bool builtin_contains(int arg_count, Value *args, Value *out_result) {
	if (arg_count != 2) {
		fprintf(stderr, "contains expects 2 arguments.\n");
		return false;
	}

	if (!IS_OBJ(args[0])) {
		fprintf(stderr, "contains expects a collection or string.\n");
		return false;
	}

	switch (OBJ_TYPE(AS_OBJ(args[0]))) {
		case OBJ_STRING: {
			if (!IS_OBJ(args[1]) || OBJ_TYPE(AS_OBJ(args[1])) != OBJ_STRING) {
				fprintf(stderr, "contains(string, string) expected.\n");
				return false;
			}
			ObjString *haystack = (ObjString *)AS_OBJ(args[0]);
			ObjString *needle = (ObjString *)AS_OBJ(args[1]);
			*out_result = BOOL_VAL(strstr(haystack->chars, needle->chars) != NULL);
			return true;
		}
		case OBJ_ARRAY: {
			ObjArray *array = (ObjArray *)AS_OBJ(args[0]);
			for (int i = 0; i < array->count; i++) {
				if (array->items[i].type == args[1].type) {
					if (array->items[i].type == VAL_NUMBER && AS_NUMBER(array->items[i]) == AS_NUMBER(args[1])) {
						*out_result = BOOL_VAL(true);
						return true;
					}
					if (array->items[i].type == VAL_BOOL && AS_BOOL(array->items[i]) == AS_BOOL(args[1])) {
						*out_result = BOOL_VAL(true);
						return true;
					}
					if (array->items[i].type == VAL_NIL && IS_NIL(args[1])) {
						*out_result = BOOL_VAL(true);
						return true;
					}
					if (array->items[i].type == VAL_OBJ && args[1].type == VAL_OBJ && AS_OBJ(array->items[i]) == AS_OBJ(args[1])) {
						*out_result = BOOL_VAL(true);
						return true;
					}
				}
			}
			*out_result = BOOL_VAL(false);
			return true;
		}
		case OBJ_MAP: {
			if (!IS_OBJ(args[1]) || OBJ_TYPE(AS_OBJ(args[1])) != OBJ_STRING) {
				fprintf(stderr, "contains(map, string) expected.\n");
				return false;
			}
			ObjMap *map = (ObjMap *)AS_OBJ(args[0]);
			ObjString *key = (ObjString *)AS_OBJ(args[1]);
			Value unused;
			*out_result = BOOL_VAL(obj_map_get(map, key->chars, key->length, &unused));
			return true;
		}
		default:
			fprintf(stderr, "contains expects a string, array, or map.\n");
			return false;
	}
}

static bool builtin_keys(int arg_count, Value *args, Value *out_result) {
	if (arg_count != 1 || !IS_OBJ(args[0]) || OBJ_TYPE(AS_OBJ(args[0])) != OBJ_MAP) {
		fprintf(stderr, "keys expects 1 map argument.\n");
		return false;
	}

	ObjMap *map = (ObjMap *)AS_OBJ(args[0]);
	Value *items = NULL;
	if (map->count > 0) {
		items = (Value *)malloc(sizeof(Value) * (size_t)map->count);
		if (items == NULL) {
			fprintf(stderr, "piko: out of memory\n");
			exit(1);
		}
		for (int i = 0; i < map->count; i++) {
			ObjString *copy = obj_string_copy(map->entries[i].key->chars, map->entries[i].key->length);
			items[i] = OBJ_VAL(copy);
		}
	}

	*out_result = OBJ_VAL(obj_array_new(items, map->count));
	free(items);
	return true;
}

static bool builtin_values(int arg_count, Value *args, Value *out_result) {
	if (arg_count != 1 || !IS_OBJ(args[0]) || OBJ_TYPE(AS_OBJ(args[0])) != OBJ_MAP) {
		fprintf(stderr, "values expects 1 map argument.\n");
		return false;
	}

	ObjMap *map = (ObjMap *)AS_OBJ(args[0]);
	Value *items = NULL;
	if (map->count > 0) {
		items = (Value *)malloc(sizeof(Value) * (size_t)map->count);
		if (items == NULL) {
			fprintf(stderr, "piko: out of memory\n");
			exit(1);
		}
		for (int i = 0; i < map->count; i++) {
			items[i] = map->entries[i].value;
		}
	}

	*out_result = OBJ_VAL(obj_array_new(items, map->count));
	free(items);
	return true;
}

static bool builtin_split(int arg_count, Value *args, Value *out_result) {
	if (arg_count != 2 || !IS_OBJ(args[0]) || OBJ_TYPE(AS_OBJ(args[0])) != OBJ_STRING || !IS_OBJ(args[1]) || OBJ_TYPE(AS_OBJ(args[1])) != OBJ_STRING) {
		fprintf(stderr, "split expects (string, string).\n");
		return false;
	}

	ObjString *text = (ObjString *)AS_OBJ(args[0]);
	ObjString *delim = (ObjString *)AS_OBJ(args[1]);
	if (delim->length == 0) {
		fprintf(stderr, "split delimiter cannot be empty.\n");
		return false;
	}

	int count = 0;
	int capacity = 0;
	Value *items = NULL;
	const char *cursor = text->chars;
	const char *match = strstr(cursor, delim->chars);
	while (match != NULL) {
		int len = (int)(match - cursor);
		if (count + 1 > capacity) {
			capacity = capacity < 4 ? 4 : capacity * 2;
			items = (Value *)realloc(items, sizeof(Value) * (size_t)capacity);
			if (items == NULL) {
				fprintf(stderr, "piko: out of memory\n");
				exit(1);
			}
		}
		items[count++] = OBJ_VAL(obj_string_copy(cursor, len));
		cursor = match + delim->length;
		match = strstr(cursor, delim->chars);
	}

	if (count + 1 > capacity) {
		capacity = capacity < 4 ? 4 : capacity * 2;
		items = (Value *)realloc(items, sizeof(Value) * (size_t)capacity);
		if (items == NULL) {
			fprintf(stderr, "piko: out of memory\n");
			exit(1);
		}
	}
	items[count++] = OBJ_VAL(obj_string_copy(cursor, (int)strlen(cursor)));

	*out_result = OBJ_VAL(obj_array_new(items, count));
	free(items);
	return true;
}

static bool builtin_join(int arg_count, Value *args, Value *out_result) {
	if (arg_count != 2 || !IS_OBJ(args[0]) || OBJ_TYPE(AS_OBJ(args[0])) != OBJ_ARRAY || !IS_OBJ(args[1]) || OBJ_TYPE(AS_OBJ(args[1])) != OBJ_STRING) {
		fprintf(stderr, "join expects (array, string).\n");
		return false;
	}

	ObjArray *array = (ObjArray *)AS_OBJ(args[0]);
	ObjString *delim = (ObjString *)AS_OBJ(args[1]);
	size_t total = 1;
	for (int i = 0; i < array->count; i++) {
		if (!IS_OBJ(array->items[i]) || OBJ_TYPE(AS_OBJ(array->items[i])) != OBJ_STRING) {
			fprintf(stderr, "join expects an array of strings.\n");
			return false;
		}
		total += (size_t)((ObjString *)AS_OBJ(array->items[i]))->length;
		if (i + 1 < array->count) {
			total += (size_t)delim->length;
		}
	}

	char *buffer = (char *)malloc(total);
	if (buffer == NULL) {
		fprintf(stderr, "piko: out of memory\n");
		exit(1);
	}
	buffer[0] = '\0';
	for (int i = 0; i < array->count; i++) {
		strcat(buffer, ((ObjString *)AS_OBJ(array->items[i]))->chars);
		if (i + 1 < array->count) {
			strcat(buffer, delim->chars);
		}
	}

	*out_result = OBJ_VAL(obj_string_copy(buffer, (int)strlen(buffer)));
	free(buffer);
	return true;
}

static bool builtin_trim(int arg_count, Value *args, Value *out_result) {
	if (arg_count != 1 || !IS_OBJ(args[0]) || OBJ_TYPE(AS_OBJ(args[0])) != OBJ_STRING) {
		fprintf(stderr, "trim expects 1 string argument.\n");
		return false;
	}

	ObjString *string = (ObjString *)AS_OBJ(args[0]);
	int start = 0;
	int end = string->length;
	while (start < end && isspace((unsigned char)string->chars[start])) {
		start++;
	}
	while (end > start && isspace((unsigned char)string->chars[end - 1])) {
		end--;
	}

	*out_result = OBJ_VAL(obj_string_copy(string->chars + start, end - start));
	return true;
}

static bool builtin_range(int arg_count, Value *args, Value *out_result) {
	if (arg_count != 1 || !IS_NUMBER(args[0])) {
		fprintf(stderr, "range expects 1 numeric argument.\n");
		return false;
	}

	int end = (int)AS_NUMBER(args[0]);
	if (end < 0) {
		fprintf(stderr, "range expects a non-negative number.\n");
		return false;
	}

	Value *items = NULL;
	if (end > 0) {
		items = (Value *)malloc(sizeof(Value) * (size_t)end);
		if (items == NULL) {
			fprintf(stderr, "piko: out of memory\n");
			exit(1);
		}
		for (int i = 0; i < end; i++) {
			items[i] = NUMBER_VAL((double)i);
		}
	}

	*out_result = OBJ_VAL(obj_array_new(items, end));
	free(items);
	return true;
}

void builtins_install(VM *vm) {
	vm_define_native(vm, "len", 1, builtin_len);
	vm_define_native(vm, "push", 2, builtin_push);
	vm_define_native(vm, "pop", 1, builtin_pop);
	vm_define_native(vm, "type", 1, builtin_type);
	vm_define_native(vm, "str", 1, builtin_str);
	vm_define_native(vm, "int", 1, builtin_int);
	vm_define_native(vm, "float", 1, builtin_float);
	vm_define_native(vm, "clock", 0, builtin_clock);
	vm_define_native(vm, "contains", 2, builtin_contains);
	vm_define_native(vm, "keys", 1, builtin_keys);
	vm_define_native(vm, "values", 1, builtin_values);
	vm_define_native(vm, "split", 2, builtin_split);
	vm_define_native(vm, "join", 2, builtin_join);
	vm_define_native(vm, "trim", 1, builtin_trim);
	vm_define_native(vm, "range", 1, builtin_range);
}
