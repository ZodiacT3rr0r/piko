#include "builtins.h"

#include "object.h"

static bool builtin_len(int arg_count, Value *args, Value *out_result) {
	if (arg_count != 1) {
		fprintf(stderr, "len expects 1 argument.\n");
		return false;
	}

	if (!IS_OBJ(args[0]) || OBJ_TYPE(AS_OBJ(args[0])) != OBJ_STRING) {
		fprintf(stderr, "len expects a string.\n");
		return false;
	}

	ObjString *string = (ObjString *)AS_OBJ(args[0]);
	*out_result = NUMBER_VAL((double)string->length);
	return true;
}

void builtins_install(VM *vm) {
	vm_define_native(vm, "len", 1, builtin_len);
}
