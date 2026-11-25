#include "object.h"

#include "memory.h"

static Obj *allocate_obj(size_t size, ObjType type) {
	Obj *obj = (Obj *)piko_reallocate(NULL, 0, size);
	obj->type = type;
	gc_register_object(obj, size);
	return obj;
}

ObjString *obj_string_copy(const char *chars, int length) {
	uint32_t hash = table_hash_string(chars, length);
	ObjString *interned = gc_intern_find(chars, length, hash);
	if (interned != NULL) {
		return interned;
	}

	ObjString *string = (ObjString *)allocate_obj(sizeof(ObjString), OBJ_STRING);
	string->length = length;
	string->hash = hash;
	string->chars = (char *)piko_reallocate(NULL, 0, (size_t)length + 1);
	if (length > 0) {
		memcpy(string->chars, chars, (size_t)length);
	}
	string->chars[length] = '\0';

	gc_intern_add(string);
	return string;
}

ObjFunction *obj_function_new(const char *name, int arity) {
	ObjFunction *function = (ObjFunction *)allocate_obj(sizeof(ObjFunction), OBJ_FUNCTION);
	size_t name_len = strlen(name) + 1;
	function->name = (char *)piko_reallocate(NULL, 0, name_len);
	memcpy(function->name, name, name_len);
	function->arity = arity;
	chunk_init(&function->chunk);
	return function;
}

ObjClosure *obj_closure_new(ObjFunction *function, int capture_count) {
	ObjClosure *closure = (ObjClosure *)allocate_obj(sizeof(ObjClosure), OBJ_CLOSURE);
	closure->function = function;
	closure->capture_count = capture_count;
	if (capture_count > 0) {
		size_t bytes = (size_t)capture_count * sizeof(struct ObjUpvalue *);
		closure->captures = (struct ObjUpvalue **)piko_reallocate(NULL, 0, bytes);
		memset(closure->captures, 0, bytes);
	} else {
		closure->captures = NULL;
	}
	return closure;
}

ObjNative *obj_native_new(const char *name, int arity, NativeFn function) {
	ObjNative *native = (ObjNative *)allocate_obj(sizeof(ObjNative), OBJ_NATIVE);
	native->name = name;
	native->arity = arity;
	native->function = function;
	return native;
}

ObjArray *obj_array_new(Value *items, int count) {
	ObjArray *arr = (ObjArray *)allocate_obj(sizeof(ObjArray), OBJ_ARRAY);
	arr->count = count;
	arr->capacity = count > 0 ? (count < 8 ? 8 : count) : 0;
	if (arr->capacity > 0) {
		arr->items = (Value *)piko_reallocate(NULL, 0, sizeof(Value) * (size_t)arr->capacity);
		for (int i = 0; i < count; i++) {
			arr->items[i] = items[i];
		}
	} else {
		arr->items = NULL;
	}
	return arr;
}

void obj_array_push(ObjArray *array, Value value) {
	if (array->count + 1 > array->capacity) {
		int old_cap = array->capacity;
		int new_cap = old_cap < 8 ? 8 : old_cap * 2;
		array->items = (Value *)piko_reallocate(
			array->items,
			sizeof(Value) * (size_t)old_cap,
			sizeof(Value) * (size_t)new_cap);
		array->capacity = new_cap;
	}
	array->items[array->count++] = value;
}

ObjUpvalue *obj_upvalue_new(Value *location) {
	ObjUpvalue *up = (ObjUpvalue *)allocate_obj(sizeof(ObjUpvalue), OBJ_UPVALUE);
	up->location = location;
	up->closed = NIL_VAL;
	up->next = NULL;
	return up;
}

ObjMap *obj_map_new(void) {
	ObjMap *map = (ObjMap *)allocate_obj(sizeof(ObjMap), OBJ_MAP);
	table_init(&map->table);
	map->count = 0;
	map->keys_order = NULL;
	map->order_count = 0;
	map->order_capacity = 0;
	return map;
}

static void map_append_order(ObjMap *map, ObjString *key) {
	if (map->order_count + 1 > map->order_capacity) {
		int old_cap = map->order_capacity;
		int new_cap = old_cap < 8 ? 8 : old_cap * 2;
		map->keys_order = (ObjString **)piko_reallocate(
			map->keys_order,
			sizeof(ObjString *) * (size_t)old_cap,
			sizeof(ObjString *) * (size_t)new_cap);
		map->order_capacity = new_cap;
	}
	map->keys_order[map->order_count++] = key;
}

bool obj_map_set_key(ObjMap *map, ObjString *key, Value value) {
	bool is_new = table_set(&map->table, key, value);
	if (is_new) {
		map->count++;
		map_append_order(map, key);
	}
	return true;
}

bool obj_map_set(ObjMap *map, const char *key, int length, Value value) {
	ObjString *key_obj = obj_string_copy(key, length);
	return obj_map_set_key(map, key_obj, value);
}

bool obj_map_get_key(ObjMap *map, ObjString *key, Value *out_value) {
	return table_get(&map->table, key, out_value);
}

bool obj_map_get(ObjMap *map, const char *key, int length, Value *out_value) {
	ObjString *key_obj = obj_string_copy(key, length);
	return table_get(&map->table, key_obj, out_value);
}

bool obj_map_remove(ObjMap *map, const char *key, int length) {
	ObjString *key_obj = obj_string_copy(key, length);
	if (!table_delete(&map->table, key_obj)) {
		return false;
	}
	for (int i = 0; i < map->order_count; i++) {
		ObjString *k = map->keys_order[i];
		if (k == key_obj || (k->length == length && memcmp(k->chars, key, (size_t)length) == 0)) {
			for (int j = i; j < map->order_count - 1; j++) {
				map->keys_order[j] = map->keys_order[j + 1];
			}
			map->order_count--;
			break;
		}
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
			int printed = 0;
			for (int i = 0; i < map->order_count; i++) {
				ObjString *key = map->keys_order[i];
				Value val;
				if (!table_get(&map->table, key, &val)) {
					continue;
				}
				if (printed > 0) {
					printf(", ");
				}
				printf("%s:", key->chars);
				print_value(val);
				printed++;
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
				if (i + 1 < a->count) {
					printf(", ");
				}
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
			piko_reallocate(string->chars, (size_t)string->length + 1, 0);
			piko_reallocate(string, sizeof(ObjString), 0);
			break;
		}
		case OBJ_FUNCTION: {
			ObjFunction *function = (ObjFunction *)obj;
			size_t name_len = strlen(function->name) + 1;
			piko_reallocate(function->name, name_len, 0);
			chunk_free(&function->chunk);
			piko_reallocate(function, sizeof(ObjFunction), 0);
			break;
		}
		case OBJ_CLOSURE: {
			ObjClosure *closure = (ObjClosure *)obj;
			if (closure->captures != NULL) {
				piko_reallocate(closure->captures,
					(size_t)closure->capture_count * sizeof(struct ObjUpvalue *), 0);
			}
			piko_reallocate(closure, sizeof(ObjClosure), 0);
			break;
		}
		case OBJ_UPVALUE: {
			piko_reallocate(obj, sizeof(ObjUpvalue), 0);
			break;
		}
		case OBJ_MAP: {
			ObjMap *map = (ObjMap *)obj;
			table_free(&map->table);
			if (map->keys_order != NULL) {
				piko_reallocate(map->keys_order,
					sizeof(ObjString *) * (size_t)map->order_capacity, 0);
			}
			piko_reallocate(map, sizeof(ObjMap), 0);
			break;
		}
		case OBJ_NATIVE: {
			piko_reallocate(obj, sizeof(ObjNative), 0);
			break;
		}
		case OBJ_ARRAY: {
			ObjArray *a = (ObjArray *)obj;
			if (a->items != NULL) {
				piko_reallocate(a->items, sizeof(Value) * (size_t)a->capacity, 0);
			}
			piko_reallocate(a, sizeof(ObjArray), 0);
			break;
		}
	}
}
