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

void obj_print(Obj *obj) {
	switch (obj->type) {
		case OBJ_STRING: {
			ObjString *string = (ObjString *)obj;
			printf("%s", string->chars);
			break;
		}
		case OBJ_NATIVE: {
			ObjNative *native = (ObjNative *)obj;
			printf("<native fn %s>", native->name);
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
		case OBJ_NATIVE: {
			free(obj);
			break;
		}
	}
}
