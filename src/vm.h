#ifndef PIKO_VM_H
#define PIKO_VM_H

#include "chunk.h"

typedef enum {
	VM_INTERPRET_OK,
	VM_INTERPRET_RUNTIME_ERROR,
} InterpretResult;

typedef struct {
	const Chunk *chunk;
	const uint8_t *ip;

	Value stack[256];
	Value *stack_top;

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

#endif
