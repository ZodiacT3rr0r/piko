#include "chunk.h"

static int grow_capacity(int capacity) {
	return capacity < 8 ? 8 : capacity * 2;
}

void chunk_init(Chunk *chunk) {
	chunk->count = 0;
	chunk->capacity = 0;
	chunk->code = NULL;
	chunk->lines = NULL;
	value_array_init(&chunk->constants);
}

void chunk_write(Chunk *chunk, uint8_t byte, int line) {
	if (chunk->count + 1 > chunk->capacity) {
		int old_capacity = chunk->capacity;
		chunk->capacity = grow_capacity(old_capacity);

		chunk->code = (uint8_t *)realloc(chunk->code, sizeof(uint8_t) * (size_t)chunk->capacity);
		chunk->lines = (int *)realloc(chunk->lines, sizeof(int) * (size_t)chunk->capacity);
		if (chunk->code == NULL || chunk->lines == NULL) {
			fprintf(stderr, "piko: out of memory\n");
			exit(1);
		}
	}

	chunk->code[chunk->count] = byte;
	chunk->lines[chunk->count] = line;
	chunk->count++;
}

int chunk_add_constant(Chunk *chunk, Value value) {
	value_array_write(&chunk->constants, value);
	return chunk->constants.count - 1;
}

void chunk_free(Chunk *chunk) {
	free(chunk->code);
	free(chunk->lines);
	value_array_free(&chunk->constants);
	chunk_init(chunk);
}

static int simple_instruction(const char *name, int offset) {
	printf("%-16s\n", name);
	return offset + 1;
}

static int constant_instruction(const char *name, const Chunk *chunk, int offset) {
	uint8_t constant_index = chunk->code[offset + 1];
	printf("%-16s %4d '", name, constant_index);
	print_value(chunk->constants.values[constant_index]);
	printf("'\n");
	return offset + 2;
}

void disassemble_chunk(const Chunk *chunk, const char *name) {
	printf("== %s ==\n", name);
	for (int offset = 0; offset < chunk->count;) {
		offset = disassemble_instruction(chunk, offset);
	}
}

int disassemble_instruction(const Chunk *chunk, int offset) {
	printf("%04d %4d ", offset, chunk->lines[offset]);

	uint8_t instruction = chunk->code[offset];
	switch (instruction) {
		case OP_CONSTANT:
			return constant_instruction("OP_CONSTANT", chunk, offset);
		case OP_NIL:
			return simple_instruction("OP_NIL", offset);
		case OP_TRUE:
			return simple_instruction("OP_TRUE", offset);
		case OP_FALSE:
			return simple_instruction("OP_FALSE", offset);
		case OP_POP:
			return simple_instruction("OP_POP", offset);
		case OP_GET_LOCAL:
			printf("%-16s %4d\n", "OP_GET_LOCAL", chunk->code[offset + 1]);
			return offset + 2;
		case OP_SET_LOCAL:
			printf("%-16s %4d\n", "OP_SET_LOCAL", chunk->code[offset + 1]);
			return offset + 2;
		case OP_GET_GLOBAL:
			return constant_instruction("OP_GET_GLOBAL", chunk, offset);
		case OP_DEFINE_GLOBAL:
			return constant_instruction("OP_DEFINE_GLOBAL", chunk, offset);
		case OP_SET_GLOBAL:
			return constant_instruction("OP_SET_GLOBAL", chunk, offset);
		case OP_EQUAL:
			return simple_instruction("OP_EQUAL", offset);
		case OP_GREATER:
			return simple_instruction("OP_GREATER", offset);
		case OP_LESS:
			return simple_instruction("OP_LESS", offset);
		case OP_ADD:
			return simple_instruction("OP_ADD", offset);
		case OP_SUBTRACT:
			return simple_instruction("OP_SUBTRACT", offset);
		case OP_MULTIPLY:
			return simple_instruction("OP_MULTIPLY", offset);
		case OP_DIVIDE:
			return simple_instruction("OP_DIVIDE", offset);
		case OP_MODULO:
			return simple_instruction("OP_MODULO", offset);
		case OP_NOT:
			return simple_instruction("OP_NOT", offset);
		case OP_NEGATE:
			return simple_instruction("OP_NEGATE", offset);
		case OP_CLOSURE: {
			uint8_t constant = chunk->code[offset + 1];
			uint8_t capture_count = chunk->code[offset + 2];
			printf("%-16s %4d captures=%d\n", "OP_CLOSURE", constant, capture_count);
			for (int i = 0; i < capture_count; i++) {
				printf("      |            slot=%d\n", chunk->code[offset + 3 + i]);
			}
			return offset + 3 + capture_count;
		}
		case OP_GET_UPVALUE:
			printf("%-16s %4d\n", "OP_GET_UPVALUE", chunk->code[offset + 1]);
			return offset + 2;
		case OP_SET_UPVALUE:
			printf("%-16s %4d\n", "OP_SET_UPVALUE", chunk->code[offset + 1]);
			return offset + 2;
		case OP_CALL:
			printf("%-16s %4d\n", "OP_CALL", chunk->code[offset + 1]);
			return offset + 2;
		case OP_GET_INDEX:
			return simple_instruction("OP_GET_INDEX", offset);
		case OP_PRINT:
			return simple_instruction("OP_PRINT", offset);
		case OP_RETURN:
			return simple_instruction("OP_RETURN", offset);
		default:
			printf("OP_UNKNOWN %d\n", instruction);
			return offset + 1;
	}
}
