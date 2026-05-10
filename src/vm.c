#include "vm.h"

#include <stdarg.h>

#include "object.h"
#include "value.h"

static void runtime_error(VM *vm, const char *format, ...);
static void push(VM *vm, Value value);
static Value pop(VM *vm);
static Value peek(VM *vm, int distance);
static uint8_t read_byte(VM *vm);
static Value read_constant(VM *vm);
static bool values_equal(Value a, Value b);
static bool is_falsey(Value value);
static void define_global(VM *vm, ObjString *name, Value value);
static bool get_global(VM *vm, ObjString *name, Value *out_value);
static bool set_global(VM *vm, ObjString *name, Value value);

static void ensure_global_capacity(VM *vm) {
	if (vm->globals_count + 1 <= vm->globals_capacity) {
		return;
	}

	int new_capacity = vm->globals_capacity < 8 ? 8 : vm->globals_capacity * 2;
	void *memory = realloc(vm->globals, sizeof(*vm->globals) * (size_t)new_capacity);
	if (memory == NULL) {
		fprintf(stderr, "piko: out of memory\n");
		exit(1);
	}

	vm->globals = memory;
	vm->globals_capacity = new_capacity;
}

void vm_init(VM *vm) {
	vm->chunk = NULL;
	vm->ip = NULL;
	vm->stack_top = vm->stack;
	vm->globals = NULL;
	vm->globals_count = 0;
	vm->globals_capacity = 0;
}

void vm_free(VM *vm) {
	if (vm->globals != NULL) {
		for (int i = 0; i < vm->globals_count; i++) {
			free(vm->globals[i].name);
		}
		free(vm->globals);
	}
	vm_init(vm);
}

void vm_define_native(VM *vm, const char *name, int arity, bool (*function)(int arg_count, Value *args, Value *out_result)) {
	ObjString *copy = obj_string_copy(name, (int)strlen(name));
	ObjNative *native = obj_native_new(name, arity, function);
	define_global(vm, copy, OBJ_VAL(native));
	obj_free((Obj *)copy);
}

static void push(VM *vm, Value value) {
	*vm->stack_top = value;
	vm->stack_top++;
}

static Value pop(VM *vm) {
	vm->stack_top--;
	return *vm->stack_top;
}

static Value peek(VM *vm, int distance) {
	return vm->stack_top[-1 - distance];
}

static uint8_t read_byte(VM *vm) {
	return *vm->ip++;
}

static Value read_constant(VM *vm) {
	uint8_t index = read_byte(vm);
	return vm->chunk->constants.values[index];
}

static bool is_string_value(Value value) {
	return IS_OBJ(value) && OBJ_TYPE(AS_OBJ(value)) == OBJ_STRING;
}

static bool string_equals(ObjString *a, ObjString *b) {
	if (a->length != b->length) {
		return false;
	}
	return memcmp(a->chars, b->chars, (size_t)a->length) == 0;
}

static bool values_equal(Value a, Value b) {
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
			if (is_string_value(a) && is_string_value(b)) {
				return string_equals((ObjString *)AS_OBJ(a), (ObjString *)AS_OBJ(b));
			}
			return AS_OBJ(a) == AS_OBJ(b);
	}

	return false;
}

static bool is_falsey(Value value) {
	return IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value));
}

static void runtime_error(VM *vm, const char *format, ...) {
	(void)vm;

	va_list args;
	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);
	fprintf(stderr, "\n");
}

static int find_global_index(VM *vm, ObjString *name) {
	for (int i = 0; i < vm->globals_count; i++) {
		if (vm->globals[i].length == name->length &&
			memcmp(vm->globals[i].name, name->chars, (size_t)name->length) == 0) {
			return i;
		}
	}

	return -1;
}

static void define_global(VM *vm, ObjString *name, Value value) {
	int index = find_global_index(vm, name);
	if (index >= 0) {
		vm->globals[index].value = value;
		return;
	}

	ensure_global_capacity(vm);

	int length = name->length;
	vm->globals[vm->globals_count].name = (char *)malloc((size_t)length + 1);
	if (vm->globals[vm->globals_count].name == NULL) {
		fprintf(stderr, "piko: out of memory\n");
		exit(1);
	}
	memcpy(vm->globals[vm->globals_count].name, name->chars, (size_t)length);
	vm->globals[vm->globals_count].name[length] = '\0';
	vm->globals[vm->globals_count].length = length;
	vm->globals[vm->globals_count].value = value;
	vm->globals_count++;
}

static bool get_global(VM *vm, ObjString *name, Value *out_value) {
	int index = find_global_index(vm, name);
	if (index < 0) {
		return false;
	}

	*out_value = vm->globals[index].value;
	return true;
}

static bool set_global(VM *vm, ObjString *name, Value value) {
	int index = find_global_index(vm, name);
	if (index < 0) {
		return false;
	}

	vm->globals[index].value = value;
	return true;
}

static bool binary_number_op(VM *vm, char op) {
	if (!IS_NUMBER(peek(vm, 0)) || !IS_NUMBER(peek(vm, 1))) {
		runtime_error(vm, "Operands must be numbers.");
		return false;
	}

	double right = AS_NUMBER(pop(vm));
	double left = AS_NUMBER(pop(vm));

	switch (op) {
		case '+':
			push(vm, NUMBER_VAL(left + right));
			break;
		case '-':
			push(vm, NUMBER_VAL(left - right));
			break;
		case '*':
			push(vm, NUMBER_VAL(left * right));
			break;
		case '/':
			push(vm, NUMBER_VAL(left / right));
			break;
		case '%':
			push(vm, NUMBER_VAL((double)((int64_t)left % (int64_t)right)));
			break;
	}

	return true;
}

static bool call_native(VM *vm, ObjNative *native, int arg_count) {
	if (arg_count != native->arity) {
		runtime_error(vm, "%s expects %d arguments.", native->name, native->arity);
		return false;
	}

	Value result = NIL_VAL;
	if (!native->function(arg_count, vm->stack_top - arg_count, &result)) {
		return false;
	}

	vm->stack_top -= arg_count + 1;
	push(vm, result);
	return true;
}

InterpretResult vm_interpret(VM *vm, const Chunk *chunk) {
	vm->chunk = chunk;
	vm->ip = chunk->code;
	vm->stack_top = vm->stack;

	for (;;) {
		uint8_t instruction = read_byte(vm);
		switch (instruction) {
			case OP_CONSTANT:
				push(vm, read_constant(vm));
				break;
			case OP_NIL:
				push(vm, NIL_VAL);
				break;
			case OP_TRUE:
				push(vm, BOOL_VAL(true));
				break;
			case OP_FALSE:
				push(vm, BOOL_VAL(false));
				break;
			case OP_POP:
				pop(vm);
				break;
			case OP_GET_GLOBAL: {
				Value constant = read_constant(vm);
				ObjString *name = (ObjString *)AS_OBJ(constant);
				Value value;
				if (!get_global(vm, name, &value)) {
					runtime_error(vm, "Undefined variable '%s'.", name->chars);
					return VM_INTERPRET_RUNTIME_ERROR;
				}
				push(vm, value);
				break;
			}
			case OP_DEFINE_GLOBAL: {
				Value constant = read_constant(vm);
				ObjString *name = (ObjString *)AS_OBJ(constant);
				Value value = pop(vm);
				define_global(vm, name, value);
				break;
			}
			case OP_SET_GLOBAL: {
				Value constant = read_constant(vm);
				ObjString *name = (ObjString *)AS_OBJ(constant);
				if (!set_global(vm, name, peek(vm, 0))) {
					runtime_error(vm, "Undefined variable '%s'.", name->chars);
					return VM_INTERPRET_RUNTIME_ERROR;
				}
				break;
			}
			case OP_EQUAL: {
				Value right = pop(vm);
				Value left = pop(vm);
				push(vm, BOOL_VAL(values_equal(left, right)));
				break;
			}
			case OP_GREATER: {
				if (!IS_NUMBER(peek(vm, 0)) || !IS_NUMBER(peek(vm, 1))) {
					runtime_error(vm, "Operands must be numbers.");
					return VM_INTERPRET_RUNTIME_ERROR;
				}
				double right = AS_NUMBER(pop(vm));
				double left = AS_NUMBER(pop(vm));
				push(vm, BOOL_VAL(left > right));
				break;
			}
			case OP_LESS: {
				if (!IS_NUMBER(peek(vm, 0)) || !IS_NUMBER(peek(vm, 1))) {
					runtime_error(vm, "Operands must be numbers.");
					return VM_INTERPRET_RUNTIME_ERROR;
				}
				double right = AS_NUMBER(pop(vm));
				double left = AS_NUMBER(pop(vm));
				push(vm, BOOL_VAL(left < right));
				break;
			}
			case OP_ADD:
				if (!binary_number_op(vm, '+')) {
					return VM_INTERPRET_RUNTIME_ERROR;
				}
				break;
			case OP_SUBTRACT:
				if (!binary_number_op(vm, '-')) {
					return VM_INTERPRET_RUNTIME_ERROR;
				}
				break;
			case OP_MULTIPLY:
				if (!binary_number_op(vm, '*')) {
					return VM_INTERPRET_RUNTIME_ERROR;
				}
				break;
			case OP_DIVIDE:
				if (!binary_number_op(vm, '/')) {
					return VM_INTERPRET_RUNTIME_ERROR;
				}
				break;
			case OP_MODULO:
				if (!binary_number_op(vm, '%')) {
					return VM_INTERPRET_RUNTIME_ERROR;
				}
				break;
			case OP_NOT:
				push(vm, BOOL_VAL(is_falsey(pop(vm))));
				break;
			case OP_CALL: {
				int arg_count = (int)read_byte(vm);
				Value callee = peek(vm, arg_count);
				if (!IS_OBJ(callee) || OBJ_TYPE(AS_OBJ(callee)) != OBJ_NATIVE) {
					runtime_error(vm, "Can only call native functions in this build.");
					return VM_INTERPRET_RUNTIME_ERROR;
				}
				if (!call_native(vm, (ObjNative *)AS_OBJ(callee), arg_count)) {
					return VM_INTERPRET_RUNTIME_ERROR;
				}
				break;
			}
			case OP_GET_INDEX: {
				Value index = pop(vm);
				Value array = pop(vm);
				if (!IS_OBJ(array) || OBJ_TYPE(AS_OBJ(array)) != OBJ_ARRAY) {
					runtime_error(vm, "Indexing supported only on arrays.");
					return VM_INTERPRET_RUNTIME_ERROR;
				}
				if (!IS_NUMBER(index)) {
					runtime_error(vm, "Index must be a number.");
					return VM_INTERPRET_RUNTIME_ERROR;
				}
				ObjArray *a = (ObjArray *)AS_OBJ(array);
				int idx = (int)AS_NUMBER(index);
				if (idx < 0 || idx >= a->count) {
					runtime_error(vm, "Index out of bounds.");
					return VM_INTERPRET_RUNTIME_ERROR;
				}
				push(vm, a->items[idx]);
				break;
			}
			case OP_NEGATE:
				if (!IS_NUMBER(peek(vm, 0))) {
					runtime_error(vm, "Operand must be a number.");
					return VM_INTERPRET_RUNTIME_ERROR;
				}
				push(vm, NUMBER_VAL(-AS_NUMBER(pop(vm))));
				break;
			case OP_PRINT:
				print_value(pop(vm));
				printf("\n");
				break;
			case OP_RETURN:
				return VM_INTERPRET_OK;
			default:
				runtime_error(vm, "Unknown opcode %d.", instruction);
				return VM_INTERPRET_RUNTIME_ERROR;
		}
	}
}
