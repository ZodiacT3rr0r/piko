#include "object.h"

ObjString *obj_string_copy(const char *chars, int length) {
	ObjString *string = (ObjString *)malloc(sizeof(ObjString));
	if (string == NULL) {
		fprintf(stderr, "piko: out of memory\n");
		exit(1);
	}

	string->obj.type = OBJ_STRING;
	string->length = length;
	string->chars = (char *)malloc((size_t)length + 1);
	if (string->chars == NULL) {
		free(string);
		fprintf(stderr, "piko: out of memory\n");
		exit(1);
	}

	memcpy(string->chars, chars, (size_t)length);
	string->chars[length] = '\0';
	return string;
}

ObjFunction *obj_function_new(const char *name, int arity) {
	ObjFunction *function = (ObjFunction *)malloc(sizeof(ObjFunction));
	if (function == NULL) {
		fprintf(stderr, "piko: out of memory\n");
		exit(1);
	}

	function->obj.type = OBJ_FUNCTION;
	function->name = (char *)malloc(strlen(name) + 1);
	if (function->name == NULL) {
		free(function);
		fprintf(stderr, "piko: out of memory\n");
		exit(1);
	}
	strcpy(function->name, name);
	function->arity = arity;
	chunk_init(&function->chunk);
	return function;
}

ObjClosure *obj_closure_new(ObjFunction *function, int capture_count) {
	ObjClosure *closure = (ObjClosure *)malloc(sizeof(ObjClosure));
	if (closure == NULL) {
		fprintf(stderr, "piko: out of memory\n");
		exit(1);
	}

	closure->obj.type = OBJ_CLOSURE;
	closure->function = function;
	closure->capture_count = capture_count;
	if (capture_count > 0) {
		closure->captures = (struct ObjUpvalue **)calloc((size_t)capture_count, sizeof(struct ObjUpvalue *));
		if (closure->captures == NULL) {
			free(closure);
			fprintf(stderr, "piko: out of memory\n");
			exit(1);
		}
	} else {
		closure->captures = NULL;
	}

	return closure;
}

ObjNative *obj_native_new(const char *name, int arity, bool (*function)(int arg_count, Value *args, Value *out_result)) {
	ObjNative *native = (ObjNative *)malloc(sizeof(ObjNative));
	if (native == NULL) {
		fprintf(stderr, "piko: out of memory\n");
		exit(1);
	}

	native->obj.type = OBJ_NATIVE;
	native->name = name;
	native->arity = arity;
	native->function = function;
	return native;
}

ObjArray *obj_array_new(Value *items, int count) {
	ObjArray *arr = (ObjArray *)malloc(sizeof(ObjArray));
	if (arr == NULL) {
		fprintf(stderr, "piko: out of memory\n");
		exit(1);
	}

	arr->obj.type = OBJ_ARRAY;
	arr->count = count;
	if (count > 0) {
		arr->items = (Value *)malloc(sizeof(Value) * (size_t)count);
		if (arr->items == NULL) {
			free(arr);
			fprintf(stderr, "piko: out of memory\n");
			exit(1);
		}
		for (int i = 0; i < count; i++) {
			arr->items[i] = items[i];
		}
	} else {
		arr->items = NULL;
	}

	return arr;
}

ObjUpvalue *obj_upvalue_new(Value *location) {
	ObjUpvalue *up = (ObjUpvalue *)malloc(sizeof(ObjUpvalue));
	if (up == NULL) {
		fprintf(stderr, "piko: out of memory\n");
		exit(1);
	}

	up->obj.type = OBJ_UPVALUE;
	up->location = location;
	up->closed = NIL_VAL;
	up->next = NULL;
	return up;
}

static int map_find_entry(ObjMap *map, const char *key, int length) {
	for (int i = 0; i < map->count; i++) {
		ObjString *entry_key = map->entries[i].key;
		if (entry_key->length == length && memcmp(entry_key->chars, key, (size_t)length) == 0) {
			return i;
		}
	}
	return -1;
}

static void ensure_map_capacity(ObjMap *map) {
	if (map->count + 1 <= map->capacity) {
		return;
	}

	int new_capacity = map->capacity < 8 ? 8 : map->capacity * 2;
	void *memory = realloc(map->entries, sizeof(*map->entries) * (size_t)new_capacity);
	if (memory == NULL) {
		fprintf(stderr, "piko: out of memory\n");
		exit(1);
	}

	map->entries = memory;
	map->capacity = new_capacity;
}

ObjMap *obj_map_new(void) {
	ObjMap *map = (ObjMap *)malloc(sizeof(ObjMap));
	if (map == NULL) {
		fprintf(stderr, "piko: out of memory\n");
		exit(1);
	}

	map->obj.type = OBJ_MAP;
	map->count = 0;
	map->capacity = 0;
	map->entries = NULL;
	return map;
}

bool obj_map_set(ObjMap *map, const char *key, int length, Value value) {
	int index = map_find_entry(map, key, length);
	if (index >= 0) {
		map->entries[index].value = value;
		return true;
	}

	ensure_map_capacity(map);
	ObjString *copy = obj_string_copy(key, length);
	map->entries[map->count].key = copy;
	map->entries[map->count].value = value;
	map->count++;
	return true;
}

bool obj_map_get(ObjMap *map, const char *key, int length, Value *out_value) {
	int index = map_find_entry(map, key, length);
	if (index < 0) {
		return false;
	}

	*out_value = map->entries[index].value;
	return true;
}

bool obj_map_remove(ObjMap *map, const char *key, int length) {
	int index = map_find_entry(map, key, length);
	if (index < 0) {
		return false;
	}

	free(map->entries[index].key->chars);
	free(map->entries[index].key);
	for (int i = index; i < map->count - 1; i++) {
		map->entries[i] = map->entries[i + 1];
	}
	map->count--;
	return true;
}

void obj_print(Obj *obj) {
	switch (obj->type) {
		case OBJ_STRING: {
			ObjString *string = (ObjString *)obj;
			printf("%s", string->chars);
			break;
		}
		case OBJ_FUNCTION: {
			ObjFunction *function = (ObjFunction *)obj;
			printf("<fn %s>", function->name);
			break;
		}
		case OBJ_CLOSURE: {
			ObjClosure *closure = (ObjClosure *)obj;
			printf("<closure %s>", closure->function->name);
			break;
		}
		case OBJ_MAP: {
			ObjMap *map = (ObjMap *)obj;
			printf("{");
			for (int i = 0; i < map->count; i++) {
				printf("%s:", map->entries[i].key->chars);
				print_value(map->entries[i].value);
				if (i + 1 < map->count) {
					printf(", ");
				}
			}
			printf("}");
			break;
		}
		case OBJ_NATIVE: {
			ObjNative *native = (ObjNative *)obj;
			printf("<native fn %s>", native->name);
			break;
		}
		case OBJ_UPVALUE: {
			printf("<upvalue>");
			break;
		}
			case OBJ_ARRAY: {
				ObjArray *a = (ObjArray *)obj;
				printf("[");
				for (int i = 0; i < a->count; i++) {
					print_value(a->items[i]);
					if (i + 1 < a->count) printf(", ");
				}
				printf("]");
				break;
			}
	}
}

void obj_free(Obj *obj) {
	switch (obj->type) {
		case OBJ_STRING: {
			ObjString *string = (ObjString *)obj;
			free(string->chars);
			free(string);
			break;
		}
		case OBJ_FUNCTION: {
			ObjFunction *function = (ObjFunction *)obj;
			free(function->name);
			chunk_free(&function->chunk);
			free(function);
			break;
		}
		case OBJ_CLOSURE: {
			ObjClosure *closure = (ObjClosure *)obj;
			free(closure->captures);
			free(closure);
			break;
		}
		case OBJ_UPVALUE: {
			free(obj);
			break;
		}
		case OBJ_MAP: {
			ObjMap *map = (ObjMap *)obj;
			for (int i = 0; i < map->count; i++) {
				free(map->entries[i].key->chars);
				free(map->entries[i].key);
				if (IS_OBJ(map->entries[i].value)) {
					obj_free(AS_OBJ(map->entries[i].value));
				}
			}
			free(map->entries);
			free(map);
			break;
		}
		case OBJ_NATIVE: {
			free(obj);
			break;
		}
			case OBJ_ARRAY: {
				ObjArray *a = (ObjArray *)obj;
				if (a->items != NULL) {
					for (int i = 0; i < a->count; i++) {
						if (IS_OBJ(a->items[i])) {
							obj_free(AS_OBJ(a->items[i]));
						}
					}
					free(a->items);
				}
				free(a);
				break;
			}
	}
}
