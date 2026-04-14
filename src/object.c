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

void obj_print(Obj *obj) {
	switch (obj->type) {
		case OBJ_STRING: {
			ObjString *string = (ObjString *)obj;
			printf("%s", string->chars);
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
	}
}
