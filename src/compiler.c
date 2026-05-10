#include "compiler.h"

#include "object.h"

typedef struct {
	Chunk *chunk;
	bool had_error;
} Compiler;

static void compile_expression(Compiler *c, Node *node);
static void compile_statement(Compiler *c, Node *node);
static void compile_call(Compiler *c, Node *node);

static void compile_error(Compiler *c, int line, const char *message) {
	fprintf(stderr, "[line %d] Compile error: %s\n", line, message);
	c->had_error = true;
}

static void emit_byte(Compiler *c, uint8_t byte, int line) {
	chunk_write(c->chunk, byte, line);
}

static void emit_bytes(Compiler *c, uint8_t first, uint8_t second, int line) {
	emit_byte(c, first, line);
	emit_byte(c, second, line);
}

static int make_constant(Compiler *c, Value value, int line) {
	int constant = chunk_add_constant(c->chunk, value);
	if (constant > 255) {
		compile_error(c, line, "Too many constants in one chunk.");
		return 0;
	}
	return constant;
}

static void emit_constant(Compiler *c, Value value, int line) {
	int constant = make_constant(c, value, line);
	emit_bytes(c, OP_CONSTANT, (uint8_t)constant, line);
}

static void emit_global(Compiler *c, OpCode op, const char *name, int line) {
	int length = (int)strlen(name);
	ObjString *str = obj_string_copy(name, length);
	int constant = make_constant(c, OBJ_VAL(str), line);
	emit_bytes(c, (uint8_t)op, (uint8_t)constant, line);
}

static void compile_binary(Compiler *c, Node *node) {
	compile_expression(c, node->as.binary.left);
	compile_expression(c, node->as.binary.right);

	switch (node->as.binary.op) {
		case TOK_PLUS:
			emit_byte(c, OP_ADD, node->line);
			break;
		case TOK_MINUS:
			emit_byte(c, OP_SUBTRACT, node->line);
			break;
		case TOK_STAR:
			emit_byte(c, OP_MULTIPLY, node->line);
			break;
		case TOK_SLASH:
			emit_byte(c, OP_DIVIDE, node->line);
			break;
		case TOK_PERCENT:
			emit_byte(c, OP_MODULO, node->line);
			break;
		case TOK_EQ_EQ:
			emit_byte(c, OP_EQUAL, node->line);
			break;
		case TOK_BANG_EQ:
			emit_byte(c, OP_EQUAL, node->line);
			emit_byte(c, OP_NOT, node->line);
			break;
		case TOK_GT:
			emit_byte(c, OP_GREATER, node->line);
			break;
		case TOK_GT_EQ:
			emit_byte(c, OP_LESS, node->line);
			emit_byte(c, OP_NOT, node->line);
			break;
		case TOK_LT:
			emit_byte(c, OP_LESS, node->line);
			break;
		case TOK_LT_EQ:
			emit_byte(c, OP_GREATER, node->line);
			emit_byte(c, OP_NOT, node->line);
			break;
		default:
			compile_error(c, node->line, "Unsupported binary operator.");
			break;
	}
}

static void compile_unary(Compiler *c, Node *node) {
	compile_expression(c, node->as.unary.right);

	switch (node->as.unary.op) {
		case TOK_MINUS:
			emit_byte(c, OP_NEGATE, node->line);
			break;
		case TOK_BANG:
			emit_byte(c, OP_NOT, node->line);
			break;
		default:
			compile_error(c, node->line, "Unsupported unary operator.");
			break;
	}
}

static void compile_call(Compiler *c, Node *node) {
	compile_expression(c, node->as.call.callee);
	for (int i = 0; i < node->as.call.arg_count; i++) {
		compile_expression(c, node->as.call.args[i]);
	}
	emit_bytes(c, OP_CALL, (uint8_t)node->as.call.arg_count, node->line);
}

static void compile_expression(Compiler *c, Node *node) {
	if (node == NULL) {
		compile_error(c, 0, "Missing expression node.");
		return;
	}

	switch (node->type) {
		case NODE_INT_LIT:
			emit_constant(c, NUMBER_VAL((double)node->as.int_val), node->line);
			break;
		case NODE_FLOAT_LIT:
			emit_constant(c, NUMBER_VAL(node->as.float_val), node->line);
			break;
		case NODE_BOOL_LIT:
			emit_byte(c, node->as.bool_val ? OP_TRUE : OP_FALSE, node->line);
			break;
		case NODE_NIL_LIT:
			emit_byte(c, OP_NIL, node->line);
			break;
		case NODE_STRING_LIT: {
			ObjString *str = obj_string_copy(node->as.string_val.chars, node->as.string_val.length);
			emit_constant(c, OBJ_VAL(str), node->line);
			break;
		}
		case NODE_IDENT: {
			ObjString *name = obj_string_copy(node->as.ident.name, node->as.ident.length);
			int constant = make_constant(c, OBJ_VAL(name), node->line);
			emit_bytes(c, OP_GET_GLOBAL, (uint8_t)constant, node->line);
			break;
		}
		case NODE_GROUPING:
			compile_expression(c, node->as.grouping.expr);
			break;
		case NODE_UNARY:
			compile_unary(c, node);
			break;
		case NODE_CALL:
			compile_call(c, node);
			break;
		case NODE_BINARY:
			compile_binary(c, node);
			break;
		default:
			compile_error(c, node->line, "Unsupported expression node.");
			break;
	}
}

static void compile_statement(Compiler *c, Node *node) {
	if (node == NULL) {
		compile_error(c, 0, "Missing statement node.");
		return;
	}

	switch (node->type) {
		case NODE_EXPR_STMT:
			compile_expression(c, node->as.expr_stmt.expr);
			emit_byte(c, OP_POP, node->line);
			break;
		case NODE_PRINT_STMT:
			compile_expression(c, node->as.print_stmt.expr);
			emit_byte(c, OP_PRINT, node->line);
			break;
		case NODE_LET:
			compile_expression(c, node->as.let_stmt.init);
			emit_global(c, OP_DEFINE_GLOBAL, node->as.let_stmt.name, node->line);
			break;
		default:
			compile_error(c, node->line, "Unsupported statement node.");
			break;
	}
}

bool compile_program(Node *program, Chunk *out_chunk) {
	if (program == NULL || program->type != NODE_PROGRAM) {
		fprintf(stderr, "piko: compiler expected a program node\n");
		return false;
	}

	Compiler c;
	c.chunk = out_chunk;
	c.had_error = false;

	chunk_init(out_chunk);

	for (int i = 0; i < program->as.program.count; i++) {
		compile_statement(&c, program->as.program.items[i]);
	}

	emit_byte(&c, OP_RETURN, program->line);
	return !c.had_error;
}
