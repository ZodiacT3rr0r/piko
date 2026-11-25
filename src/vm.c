#include "vm.h"

#include <stdarg.h>

#include "memory.h"
#include "object.h"
#include "value.h"

static void runtime_error(VM *vm, const char *format, ...);
static bool push(VM *vm, Value value);
static Value pop(VM *vm);
static Value peek(VM *vm, int distance);
static CallFrame *current_frame(VM *vm);
static uint8_t read_byte(VM *vm);
static uint16_t read_short(VM *vm);
static Value read_constant(VM *vm);
static bool is_falsey(Value value);
static InterpretResult vm_run(VM *vm, int stop_frame_count);

void vm_init(VM *vm) {
	vm->frame_count = 0;
	vm->stack_top = vm->stack;
	vm->open_upvalues = NULL;
	vm->temp_root_count = 0;
	table_init(&vm->globals);
	table_init(&vm->strings);
	gc_bind_vm(vm);
}

void vm_free(VM *vm) {
	gc_set_enabled(false);
	table_free(&vm->globals);
	table_free(&vm->strings);
	vm->open_upvalues = NULL;
	vm->frame_count = 0;
	vm->stack_top = vm->stack;
	vm->temp_root_count = 0;
	gc_unbind_vm(vm);
	gc_free_all();
}

void vm_push_temp_root(VM *vm, Value value) {
	if (vm->temp_root_count >= TEMP_ROOTS_MAX) {
		fprintf(stderr, "piko: GC temp root stack overflow\n");
		exit(1);
	}
	vm->temp_roots[vm->temp_root_count++] = value;
}

void vm_pop_temp_root(VM *vm) {
	if (vm->temp_root_count > 0) {
		vm->temp_root_count--;
	}
}

static ObjUpvalue *capture_upvalue(VM *vm, Value *local) {
	ObjUpvalue *prev = NULL;
	ObjUpvalue *up = vm->open_upvalues;

	while (up != NULL && up->location > local) {
		prev = up;
		up = up->next;
	}

	if (up != NULL && up->location == local) {
		return up;
	}

	ObjUpvalue *created = obj_upvalue_new(local);
	created->next = up;
	if (prev == NULL) {
		vm->open_upvalues = created;
	} else {
		prev->next = created;
	}
	return created;
}

static void close_upvalues(VM *vm, Value *last) {
	while (vm->open_upvalues != NULL && vm->open_upvalues->location >= last) {
		ObjUpvalue *up = vm->open_upvalues;
		up->closed = *up->location;
		up->location = &up->closed;
		vm->open_upvalues = up->next;
	}
}

void vm_define_native(VM *vm, const char *name, int arity, NativeFn function) {
	ObjString *key = obj_string_copy(name, (int)strlen(name));
	vm_push_temp_root(vm, OBJ_VAL(key));
	ObjNative *native = obj_native_new(name, arity, function);
	vm_push_temp_root(vm, OBJ_VAL(native));
	table_set(&vm->globals, key, OBJ_VAL(native));
	vm_pop_temp_root(vm);
	vm_pop_temp_root(vm);
}

static CallFrame *current_frame(VM *vm) {
	return &vm->frames[vm->frame_count - 1];
}

static bool push(VM *vm, Value value) {
	if ((int)(vm->stack_top - vm->stack) >= STACK_MAX) {
		runtime_error(vm, "Value stack overflow.");
		return false;
	}
	*vm->stack_top = value;
	vm->stack_top++;
	return true;
}

static Value pop(VM *vm) {
	vm->stack_top--;
	return *vm->stack_top;
}

static Value peek(VM *vm, int distance) {
	return vm->stack_top[-1 - distance];
}

static uint8_t read_byte(VM *vm) {
	CallFrame *frame = current_frame(vm);
	return *frame->ip++;
}

static uint16_t read_short(VM *vm) {
	CallFrame *frame = current_frame(vm);
	frame->ip += 2;
	return (uint16_t)((frame->ip[-2] << 8) | frame->ip[-1]);
}

static Value read_constant(VM *vm) {
	CallFrame *frame = current_frame(vm);
	uint8_t index = read_byte(vm);
	return frame->chunk->constants.values[index];
}

static bool is_string_value(Value value) {
	return IS_OBJ(value) && OBJ_TYPE(AS_OBJ(value)) == OBJ_STRING;
}

static bool is_falsey(Value value) {
	return IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value));
}

static void runtime_error(VM *vm, const char *format, ...) {
	int line = 0;
	if (vm != NULL && vm->frame_count > 0) {
		CallFrame *frame = &vm->frames[vm->frame_count - 1];
		if (frame->chunk != NULL && frame->chunk->lines != NULL && frame->ip > frame->chunk->code) {
			size_t instruction = (size_t)(frame->ip - frame->chunk->code - 1);
			if ((int)instruction < frame->chunk->count) {
				line = frame->chunk->lines[instruction];
			}
		}
	}

	if (line > 0) {
		fprintf(stderr, "[line %d] Runtime error: ", line);
	} else {
		fprintf(stderr, "Runtime error: ");
	}

	va_list args;
	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);
	fprintf(stderr, "\n");

	if (vm != NULL) {
		for (int i = vm->frame_count - 1; i >= 0; i--) {
			CallFrame *frame = &vm->frames[i];
			int fline = 0;
			if (frame->chunk != NULL && frame->chunk->lines != NULL && frame->ip > frame->chunk->code) {
				size_t inst = (size_t)(frame->ip - frame->chunk->code - 1);
				if ((int)inst < frame->chunk->count) {
					fline = frame->chunk->lines[inst];
				}
			}
			const char *fn_name = "<script>";
			if (frame->closure != NULL && frame->closure->function != NULL &&
				frame->closure->function->name[0] != '\0') {
				fn_name = frame->closure->function->name;
			}
			fprintf(stderr, "  at [line %d] in %s\n", fline, fn_name);
		}
	}
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
			return push(vm, NUMBER_VAL(left + right));
		case '-':
			return push(vm, NUMBER_VAL(left - right));
		case '*':
			return push(vm, NUMBER_VAL(left * right));
		case '/':
			return push(vm, NUMBER_VAL(left / right));
		case '%': {
			int64_t rhs = (int64_t)right;
			if (rhs == 0) {
				runtime_error(vm, "Modulo by zero.");
				return false;
			}
			return push(vm, NUMBER_VAL((double)((int64_t)left % rhs)));
		}
	}

	return true;
}

static bool call_native(VM *vm, ObjNative *native, int arg_count) {
	if (arg_count != native->arity) {
		runtime_error(vm, "%s expects %d arguments.", native->name, native->arity);
		return false;
	}

	Value result = NIL_VAL;
	if (!native->function(vm, arg_count, vm->stack_top - arg_count, &result)) {
		return false;
	}

	vm->stack_top -= arg_count + 1;
	return push(vm, result);
}

static bool call_closure(VM *vm, ObjClosure *closure, int arg_count) {
	ObjFunction *function = closure->function;
	if (arg_count != function->arity) {
		runtime_error(vm, "%s expects %d arguments.", function->name, function->arity);
		return false;
	}

	if (vm->frame_count >= FRAMES_MAX) {
		runtime_error(vm, "Call stack overflow.");
		return false;
	}

	CallFrame *frame = &vm->frames[vm->frame_count++];
	frame->chunk = &function->chunk;
	frame->ip = function->chunk.code;
	frame->slots = vm->stack_top - arg_count - 1;
	frame->closure = closure;
	return true;
}

bool vm_call_value(VM *vm, Value callee, int arg_count, Value *args, Value *out_result) {
	if (!IS_OBJ(callee)) {
		runtime_error(vm, "Can only call functions.");
		return false;
	}

	switch (OBJ_TYPE(AS_OBJ(callee))) {
		case OBJ_NATIVE: {
			ObjNative *native = (ObjNative *)AS_OBJ(callee);
			if (arg_count != native->arity) {
				runtime_error(vm, "%s expects %d arguments.", native->name, native->arity);
				return false;
			}
			if (!push(vm, callee)) {
				return false;
			}
			for (int i = 0; i < arg_count; i++) {
				if (!push(vm, args[i])) {
					return false;
				}
			}
			bool ok = native->function(vm, arg_count, vm->stack_top - arg_count, out_result);
			vm->stack_top -= arg_count + 1;
			return ok;
		}
		case OBJ_CLOSURE: {
			int base_frames = vm->frame_count;
			if (!push(vm, callee)) {
				return false;
			}
			for (int i = 0; i < arg_count; i++) {
				if (!push(vm, args[i])) {
					return false;
				}
			}
			if (!call_closure(vm, (ObjClosure *)AS_OBJ(callee), arg_count)) {
				return false;
			}
			if (vm_run(vm, base_frames) != VM_INTERPRET_OK) {
				return false;
			}
			*out_result = pop(vm);
			return true;
		}
		default:
			runtime_error(vm, "Can only call functions.");
			return false;
	}
}

static InterpretResult vm_run(VM *vm, int stop_frame_count) {
	for (;;) {
		uint8_t instruction = read_byte(vm);
		switch (instruction) {
			case OP_CONSTANT:
				if (!push(vm, read_constant(vm))) {
					return VM_INTERPRET_RUNTIME_ERROR;
				}
				break;
			case OP_NIL:
				if (!push(vm, NIL_VAL)) {
					return VM_INTERPRET_RUNTIME_ERROR;
				}
				break;
			case OP_TRUE:
				if (!push(vm, BOOL_VAL(true))) {
					return VM_INTERPRET_RUNTIME_ERROR;
				}
				break;
			case OP_FALSE:
				if (!push(vm, BOOL_VAL(false))) {
					return VM_INTERPRET_RUNTIME_ERROR;
				}
				break;
			case OP_POP:
				pop(vm);
				break;
			case OP_GET_LOCAL: {
				uint8_t slot = read_byte(vm);
				if (!push(vm, current_frame(vm)->slots[slot])) {
					return VM_INTERPRET_RUNTIME_ERROR;
				}
				break;
			}
			case OP_SET_LOCAL: {
				uint8_t slot = read_byte(vm);
				current_frame(vm)->slots[slot] = peek(vm, 0);
				break;
			}
			case OP_GET_GLOBAL: {
				Value constant = read_constant(vm);
				ObjString *name = (ObjString *)AS_OBJ(constant);
				Value value;
				if (!table_get(&vm->globals, name, &value)) {
					runtime_error(vm, "Undefined variable '%s'.", name->chars);
					return VM_INTERPRET_RUNTIME_ERROR;
				}
				if (!push(vm, value)) {
					return VM_INTERPRET_RUNTIME_ERROR;
				}
				break;
			}
			case OP_DEFINE_GLOBAL: {
				Value constant = read_constant(vm);
				ObjString *name = (ObjString *)AS_OBJ(constant);
				/* Peek before table_set so value stays rooted if table_set triggers GC. */
				table_set(&vm->globals, name, peek(vm, 0));
				pop(vm);
				break;
			}
			case OP_SET_GLOBAL: {
				Value constant = read_constant(vm);
				ObjString *name = (ObjString *)AS_OBJ(constant);
				if (table_set(&vm->globals, name, peek(vm, 0))) {
					table_delete(&vm->globals, name);
					runtime_error(vm, "Undefined variable '%s'.", name->chars);
					return VM_INTERPRET_RUNTIME_ERROR;
				}
				break;
			}
			case OP_EQUAL: {
				Value right = pop(vm);
				Value left = pop(vm);
				if (!push(vm, BOOL_VAL(values_equal(left, right)))) {
					return VM_INTERPRET_RUNTIME_ERROR;
				}
				break;
			}
			case OP_GREATER: {
				if (!IS_NUMBER(peek(vm, 0)) || !IS_NUMBER(peek(vm, 1))) {
					runtime_error(vm, "Operands must be numbers.");
					return VM_INTERPRET_RUNTIME_ERROR;
				}
				double right = AS_NUMBER(pop(vm));
				double left = AS_NUMBER(pop(vm));
				if (!push(vm, BOOL_VAL(left > right))) {
					return VM_INTERPRET_RUNTIME_ERROR;
				}
				break;
			}
			case OP_LESS: {
				if (!IS_NUMBER(peek(vm, 0)) || !IS_NUMBER(peek(vm, 1))) {
					runtime_error(vm, "Operands must be numbers.");
					return VM_INTERPRET_RUNTIME_ERROR;
				}
				double right = AS_NUMBER(pop(vm));
				double left = AS_NUMBER(pop(vm));
				if (!push(vm, BOOL_VAL(left < right))) {
					return VM_INTERPRET_RUNTIME_ERROR;
				}
				break;
			}
			case OP_ADD: {
				if (is_string_value(peek(vm, 0)) && is_string_value(peek(vm, 1))) {
					ObjString *b = (ObjString *)AS_OBJ(peek(vm, 0));
					ObjString *a = (ObjString *)AS_OBJ(peek(vm, 1));
					int len = a->length + b->length;
					char *buf = (char *)malloc((size_t)len + 1);
					if (buf == NULL) {
						fprintf(stderr, "piko: out of memory\n");
						exit(1);
					}
					memcpy(buf, a->chars, (size_t)a->length);
					memcpy(buf + a->length, b->chars, (size_t)b->length);
					buf[len] = '\0';
					ObjString *result = obj_string_copy(buf, len);
					free(buf);
					pop(vm);
					pop(vm);
					if (!push(vm, OBJ_VAL(result))) {
						return VM_INTERPRET_RUNTIME_ERROR;
					}
				} else if (!binary_number_op(vm, '+')) {
					return VM_INTERPRET_RUNTIME_ERROR;
				}
				break;
			}
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
				if (!push(vm, BOOL_VAL(is_falsey(pop(vm))))) {
					return VM_INTERPRET_RUNTIME_ERROR;
				}
				break;
			case OP_NEGATE:
				if (!IS_NUMBER(peek(vm, 0))) {
					runtime_error(vm, "Operand must be a number.");
					return VM_INTERPRET_RUNTIME_ERROR;
				}
				if (!push(vm, NUMBER_VAL(-AS_NUMBER(pop(vm))))) {
					return VM_INTERPRET_RUNTIME_ERROR;
				}
				break;
			case OP_CLOSURE: {
				Value function_val = read_constant(vm);
				uint8_t capture_count = read_byte(vm);
				if (!IS_OBJ(function_val) || OBJ_TYPE(AS_OBJ(function_val)) != OBJ_FUNCTION) {
					runtime_error(vm, "Closure constant must be a function.");
					return VM_INTERPRET_RUNTIME_ERROR;
				}

				ObjFunction *function = (ObjFunction *)AS_OBJ(function_val);
				ObjClosure *closure = obj_closure_new(function, (int)capture_count);
				if (!push(vm, OBJ_VAL(closure))) {
					return VM_INTERPRET_RUNTIME_ERROR;
				}
				CallFrame *frame = current_frame(vm);
				for (int i = 0; i < (int)capture_count; i++) {
					uint8_t is_local = read_byte(vm);
					uint8_t index = read_byte(vm);
					if (is_local) {
						closure->captures[i] = capture_upvalue(vm, &frame->slots[index]);
					} else {
						if (frame->closure == NULL || index >= (uint8_t)frame->closure->capture_count) {
							runtime_error(vm, "Invalid outer upvalue capture.");
							return VM_INTERPRET_RUNTIME_ERROR;
						}
						closure->captures[i] = frame->closure->captures[index];
					}
				}
				break;
			}
			case OP_GET_UPVALUE: {
				uint8_t index = read_byte(vm);
				CallFrame *frame = current_frame(vm);
				if (frame->closure == NULL || index >= (uint8_t)frame->closure->capture_count) {
					runtime_error(vm, "Invalid upvalue read.");
					return VM_INTERPRET_RUNTIME_ERROR;
				}
				ObjUpvalue *up = frame->closure->captures[index];
				if (!push(vm, *up->location)) {
					return VM_INTERPRET_RUNTIME_ERROR;
				}
				break;
			}
			case OP_SET_UPVALUE: {
				uint8_t index = read_byte(vm);
				CallFrame *frame = current_frame(vm);
				if (frame->closure == NULL || index >= (uint8_t)frame->closure->capture_count) {
					runtime_error(vm, "Invalid upvalue write.");
					return VM_INTERPRET_RUNTIME_ERROR;
				}
				ObjUpvalue *up = frame->closure->captures[index];
				*up->location = peek(vm, 0);
				break;
			}
			case OP_CLOSE_UPVALUE: {
				close_upvalues(vm, vm->stack_top - 1);
				pop(vm);
				break;
			}
			case OP_JUMP: {
				uint16_t offset = read_short(vm);
				current_frame(vm)->ip += offset;
				break;
			}
			case OP_JUMP_IF_FALSE: {
				uint16_t offset = read_short(vm);
				if (is_falsey(peek(vm, 0))) {
					current_frame(vm)->ip += offset;
				}
				break;
			}
			case OP_LOOP: {
				uint16_t offset = read_short(vm);
				current_frame(vm)->ip -= offset;
				break;
			}
			case OP_CALL: {
				int arg_count = (int)read_byte(vm);
				Value callee = peek(vm, arg_count);
				if (!IS_OBJ(callee)) {
					runtime_error(vm, "Can only call functions.");
					return VM_INTERPRET_RUNTIME_ERROR;
				}

				switch (OBJ_TYPE(AS_OBJ(callee))) {
					case OBJ_NATIVE:
						if (!call_native(vm, (ObjNative *)AS_OBJ(callee), arg_count)) {
							return VM_INTERPRET_RUNTIME_ERROR;
						}
						break;
					case OBJ_CLOSURE:
						if (!call_closure(vm, (ObjClosure *)AS_OBJ(callee), arg_count)) {
							return VM_INTERPRET_RUNTIME_ERROR;
						}
						break;
					default:
						runtime_error(vm, "Can only call functions.");
						return VM_INTERPRET_RUNTIME_ERROR;
				}
				break;
			}
			case OP_GET_INDEX: {
				Value index = peek(vm, 0);
				Value target = peek(vm, 1);
				if (!IS_OBJ(target)) {
					runtime_error(vm, "Indexing supported only on arrays, maps, and strings.");
					return VM_INTERPRET_RUNTIME_ERROR;
				}
				switch (OBJ_TYPE(AS_OBJ(target))) {
					case OBJ_ARRAY: {
						if (!IS_NUMBER(index)) {
							runtime_error(vm, "Index must be a number.");
							return VM_INTERPRET_RUNTIME_ERROR;
						}
						ObjArray *a = (ObjArray *)AS_OBJ(target);
						int idx = (int)AS_NUMBER(index);
						if (idx < 0 || idx >= a->count) {
							runtime_error(vm, "Index out of bounds.");
							return VM_INTERPRET_RUNTIME_ERROR;
						}
						Value elem = a->items[idx];
						pop(vm);
						pop(vm);
						if (!push(vm, elem)) {
							return VM_INTERPRET_RUNTIME_ERROR;
						}
						break;
					}
					case OBJ_STRING: {
						if (!IS_NUMBER(index)) {
							runtime_error(vm, "String index must be a number.");
							return VM_INTERPRET_RUNTIME_ERROR;
						}
						ObjString *s = (ObjString *)AS_OBJ(target);
						int idx = (int)AS_NUMBER(index);
						if (idx < 0 || idx >= s->length) {
							runtime_error(vm, "String index out of bounds.");
							return VM_INTERPRET_RUNTIME_ERROR;
						}
						ObjString *ch = obj_string_copy(&s->chars[idx], 1);
						pop(vm);
						pop(vm);
						if (!push(vm, OBJ_VAL(ch))) {
							return VM_INTERPRET_RUNTIME_ERROR;
						}
						break;
					}
					case OBJ_MAP: {
						if (!IS_OBJ(index) || OBJ_TYPE(AS_OBJ(index)) != OBJ_STRING) {
							runtime_error(vm, "Map keys must be strings.");
							return VM_INTERPRET_RUNTIME_ERROR;
						}
						ObjMap *map = (ObjMap *)AS_OBJ(target);
						ObjString *key = (ObjString *)AS_OBJ(index);
						Value value = NIL_VAL;
						obj_map_get_key(map, key, &value);
						pop(vm);
						pop(vm);
						if (!push(vm, value)) {
							return VM_INTERPRET_RUNTIME_ERROR;
						}
						break;
					}
					default:
						runtime_error(vm, "Indexing supported only on arrays, maps, and strings.");
						return VM_INTERPRET_RUNTIME_ERROR;
				}
				break;
			}
			case OP_SET_INDEX: {
				Value value = peek(vm, 0);
				Value index = peek(vm, 1);
				Value target = peek(vm, 2);
				if (!IS_OBJ(target)) {
					runtime_error(vm, "Index assignment supported only on arrays and maps.");
					return VM_INTERPRET_RUNTIME_ERROR;
				}
				switch (OBJ_TYPE(AS_OBJ(target))) {
					case OBJ_ARRAY: {
						if (!IS_NUMBER(index)) {
							runtime_error(vm, "Array index must be a number.");
							return VM_INTERPRET_RUNTIME_ERROR;
						}
						ObjArray *a = (ObjArray *)AS_OBJ(target);
						int idx = (int)AS_NUMBER(index);
						if (idx < 0 || idx >= a->count) {
							runtime_error(vm, "Array index out of bounds.");
							return VM_INTERPRET_RUNTIME_ERROR;
						}
						a->items[idx] = value;
						break;
					}
					case OBJ_MAP: {
						if (!IS_OBJ(index) || OBJ_TYPE(AS_OBJ(index)) != OBJ_STRING) {
							runtime_error(vm, "Map keys must be strings.");
							return VM_INTERPRET_RUNTIME_ERROR;
						}
						ObjMap *map = (ObjMap *)AS_OBJ(target);
						ObjString *key = (ObjString *)AS_OBJ(index);
						obj_map_set_key(map, key, value);
						break;
					}
					default:
						runtime_error(vm, "Index assignment supported only on arrays and maps.");
						return VM_INTERPRET_RUNTIME_ERROR;
				}
				pop(vm);
				pop(vm);
				pop(vm);
				if (!push(vm, value)) {
					return VM_INTERPRET_RUNTIME_ERROR;
				}
				break;
			}
			case OP_BUILD_ARRAY: {
				int count = (int)read_byte(vm);
				/* Leave elements on the VM stack during allocation so GC roots them. */
				ObjArray *arr = obj_array_new(vm->stack_top - count, count);
				vm->stack_top -= count;
				if (!push(vm, OBJ_VAL(arr))) {
					return VM_INTERPRET_RUNTIME_ERROR;
				}
				break;
			}
			case OP_BUILD_MAP: {
				int pair_count = (int)read_byte(vm);
				Value *base = vm->stack_top - (pair_count * 2);
				ObjMap *map = obj_map_new();
				vm_push_temp_root(vm, OBJ_VAL(map));
				for (int i = 0; i < pair_count; i++) {
					Value k = base[i * 2];
					Value v = base[i * 2 + 1];
					if (!IS_OBJ(k) || OBJ_TYPE(AS_OBJ(k)) != OBJ_STRING) {
						vm_pop_temp_root(vm);
						runtime_error(vm, "Map keys must be strings.");
						return VM_INTERPRET_RUNTIME_ERROR;
					}
					obj_map_set_key(map, (ObjString *)AS_OBJ(k), v);
				}
				vm_pop_temp_root(vm);
				vm->stack_top -= pair_count * 2;
				if (!push(vm, OBJ_VAL(map))) {
					return VM_INTERPRET_RUNTIME_ERROR;
				}
				break;
			}
			case OP_PRINT:
				print_value(pop(vm));
				printf("\n");
				break;
			case OP_RETURN: {
				CallFrame *frame = current_frame(vm);
				if (vm->frame_count == 1 && stop_frame_count == 0) {
					vm->frame_count = 0;
					return VM_INTERPRET_OK;
				}

				Value result = NIL_VAL;
				if (vm->stack_top > frame->slots) {
					result = pop(vm);
				}

				close_upvalues(vm, frame->slots);

				vm->frame_count--;
				vm->stack_top = frame->slots;
				if (!push(vm, result)) {
					return VM_INTERPRET_RUNTIME_ERROR;
				}
				if (vm->frame_count == stop_frame_count) {
					return VM_INTERPRET_OK;
				}
				break;
			}
			default:
				runtime_error(vm, "Unknown opcode %d.", instruction);
				return VM_INTERPRET_RUNTIME_ERROR;
		}
	}
}

InterpretResult vm_interpret(VM *vm, const Chunk *chunk) {
	vm->frame_count = 1;
	vm->frames[0].chunk = chunk;
	vm->frames[0].ip = chunk->code;
	vm->frames[0].slots = vm->stack;
	vm->frames[0].closure = NULL;
	vm->stack_top = vm->stack;

	gc_set_enabled(true);
	InterpretResult res = vm_run(vm, 0);
	gc_set_enabled(false);
	return res;
}
