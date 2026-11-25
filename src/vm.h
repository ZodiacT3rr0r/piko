#ifndef PIKO_VM_H
#define PIKO_VM_H

#include "chunk.h"
#include "object.h"
#include "table.h"

#define FRAMES_MAX     64
#define STACK_MAX      256
#define TEMP_ROOTS_MAX 32

typedef enum {
	VM_INTERPRET_OK,
	VM_INTERPRET_RUNTIME_ERROR,
} InterpretResult;

typedef struct {
	const Chunk *chunk;
	const uint8_t *ip;
	Value *slots;
	ObjClosure *closure;
} CallFrame;

struct VM {
	CallFrame frames[FRAMES_MAX];
	int frame_count;

	Value stack[STACK_MAX];
	Value *stack_top;
	ObjUpvalue *open_upvalues;

	Table globals;
	Table strings;

	Value temp_roots[TEMP_ROOTS_MAX];
	int temp_root_count;
};

void vm_init(VM *vm);
void vm_free(VM *vm);
InterpretResult vm_interpret(VM *vm, const Chunk *chunk);
void vm_define_native(VM *vm, const char *name, int arity, NativeFn function);
bool vm_call_value(VM *vm, Value callee, int arg_count, Value *args, Value *out_result);
void vm_push_temp_root(VM *vm, Value value);
void vm_pop_temp_root(VM *vm);

#endif
