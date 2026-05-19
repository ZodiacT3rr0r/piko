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
		case OBJ_NATIVE: {
			ObjNative *native = (ObjNative *)obj;
			printf("<native fn %s>", native->name);
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
