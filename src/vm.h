#ifndef PIKO_VM_H
#define PIKO_VM_H

#include "chunk.h"

#define FRAMES_MAX 64
#define STACK_MAX  256

typedef struct ObjClosure ObjClosure;
typedef struct ObjUpvalue ObjUpvalue;

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

typedef struct {
	CallFrame frames[FRAMES_MAX];
	int frame_count;

	Value stack[STACK_MAX];
	Value *stack_top;
	ObjUpvalue *open_upvalues;

	struct {
		char *name;
		int length;
		Value value;
	} *globals;
	int globals_count;
	int globals_capacity;
} VM;

void vm_init(VM *vm);
void vm_free(VM *vm);
InterpretResult vm_interpret(VM *vm, const Chunk *chunk);
void vm_define_native(VM *vm, const char *name, int arity, bool (*function)(int arg_count, Value *args, Value *out_result));

#endif
