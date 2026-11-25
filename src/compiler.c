#include "compiler.h"

#include "object.h"

typedef struct {
	const char *name;
	int length;
	int depth;
	bool is_captured;
} Local;

typedef struct {
	const char *name;
	int length;
	uint8_t index;
	bool is_local;
} Upvalue;

typedef struct Compiler {
	Chunk *chunk;
	struct Compiler *enclosing;
	bool had_error;
	bool in_function;
	int scope_depth;
	Local *locals;
	int local_count;
	int local_capacity;
	Upvalue *upvalues;
	int upvalue_count;
	int upvalue_capacity;
} Compiler;

static void compile_expression(Compiler *c, Node *node);
static void compile_statement(Compiler *c, Node *node);
static void compile_call(Compiler *c, Node *node);
static void compile_function_decl(Compiler *c, Node *node);
static void compile_return(Compiler *c, Node *node);
static void compile_assign(Compiler *c, Node *node);
static void compile_if(Compiler *c, Node *node);
static void compile_while(Compiler *c, Node *node);
static void compile_for(Compiler *c, Node *node);
static void compile_block(Compiler *c, Node *node);
static void compile_logical(Compiler *c, Node *node);

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

static int emit_jump(Compiler *c, OpCode instruction, int line) {
	emit_byte(c, (uint8_t)instruction, line);
	emit_byte(c, 0xff, line);
	emit_byte(c, 0xff, line);
	return c->chunk->count - 2;
}

static void patch_jump(Compiler *c, int offset, int line) {
	int jump = c->chunk->count - offset - 2;
	if (jump > UINT16_MAX) {
		compile_error(c, line, "Too much code to jump over.");
		return;
	}
	c->chunk->code[offset] = (uint8_t)((jump >> 8) & 0xff);
	c->chunk->code[offset + 1] = (uint8_t)(jump & 0xff);
}

static void emit_loop(Compiler *c, int loop_start, int line) {
	emit_byte(c, OP_LOOP, line);
	int offset = c->chunk->count - loop_start + 2;
	if (offset > UINT16_MAX) {
		compile_error(c, line, "Loop body too large.");
		return;
	}
	emit_byte(c, (uint8_t)((offset >> 8) & 0xff), line);
	emit_byte(c, (uint8_t)(offset & 0xff), line);
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

static void ensure_local_capacity(Compiler *c) {
	if (c->local_count + 1 <= c->local_capacity) {
		return;
	}

	int new_capacity = c->local_capacity < 8 ? 8 : c->local_capacity * 2;
	void *memory = realloc(c->locals, sizeof(*c->locals) * (size_t)new_capacity);
	if (memory == NULL) {
		fprintf(stderr, "piko: out of memory\n");
		exit(1);
	}

	c->locals = memory;
	c->local_capacity = new_capacity;
}

static void compiler_init(Compiler *c, Chunk *chunk, bool in_function, Compiler *enclosing) {
	c->chunk = chunk;
	c->enclosing = enclosing;
	c->had_error = false;
	c->in_function = in_function;
	c->scope_depth = in_function ? 1 : 0;
	c->locals = NULL;
	c->local_capacity = 0;
	c->upvalues = NULL;
	c->upvalue_capacity = 0;
	c->upvalue_count = 0;
	c->local_count = 0;

	if (in_function) {
		ensure_local_capacity(c);
		c->locals[0].name = "";
		c->locals[0].length = 0;
		c->locals[0].depth = 1;
		c->locals[0].is_captured = false;
		c->local_count = 1;
	}
}

static void compiler_free(Compiler *c) {
	free(c->locals);
	free(c->upvalues);
	c->locals = NULL;
	c->upvalues = NULL;
	c->local_capacity = 0;
	c->upvalue_capacity = 0;
	c->local_count = 0;
	c->upvalue_count = 0;
}

static void begin_scope(Compiler *c) {
	c->scope_depth++;
}

static void end_scope(Compiler *c, int line) {
	c->scope_depth--;
	int min_slot = c->in_function ? 1 : 0;
	while (c->local_count > min_slot && c->locals[c->local_count - 1].depth > c->scope_depth) {
		if (c->locals[c->local_count - 1].is_captured) {
			emit_byte(c, OP_CLOSE_UPVALUE, line);
		} else {
			emit_byte(c, OP_POP, line);
		}
		c->local_count--;
	}
}

static void ensure_upvalue_capacity(Compiler *c) {
	if (c->upvalue_count + 1 <= c->upvalue_capacity) {
		return;
	}

	int new_capacity = c->upvalue_capacity < 8 ? 8 : c->upvalue_capacity * 2;
	void *memory = realloc(c->upvalues, sizeof(*c->upvalues) * (size_t)new_capacity);
	if (memory == NULL) {
		fprintf(stderr, "piko: out of memory\n");
		exit(1);
	}

	c->upvalues = memory;
	c->upvalue_capacity = new_capacity;
}

static void declare_local(Compiler *c, const char *name, int length) {
	ensure_local_capacity(c);
	c->locals[c->local_count].name = name;
	c->locals[c->local_count].length = length;
	c->locals[c->local_count].depth = c->scope_depth;
	c->locals[c->local_count].is_captured = false;
	c->local_count++;
}

static int resolve_local(Compiler *c, const char *name, int length) {
	int start = c->in_function ? 1 : 0;
	for (int i = c->local_count - 1; i >= start; i--) {
		if (c->locals[i].length == length && memcmp(c->locals[i].name, name, (size_t)length) == 0) {
			return i;
		}
	}
	return -1;
}

static int add_upvalue(Compiler *c, const char *name, int length, uint8_t index, bool is_local) {
	for (int i = 0; i < c->upvalue_count; i++) {
		if (c->upvalues[i].index == index && c->upvalues[i].is_local == is_local) {
			return i;
		}
	}

	if (c->upvalue_count >= 255) {
		return -1;
	}

	ensure_upvalue_capacity(c);
	c->upvalues[c->upvalue_count].name = name;
	c->upvalues[c->upvalue_count].length = length;
	c->upvalues[c->upvalue_count].index = index;
	c->upvalues[c->upvalue_count].is_local = is_local;
	c->upvalue_count++;
	return c->upvalue_count - 1;
}

/* Recursively resolves upvalues across arbitrary lexical nesting depths.
 * If found as a direct local in c->enclosing, marks is_local = true.
 * Otherwise recurses into c->enclosing to thread an upvalue chain (is_local = false). */
static int resolve_upvalue(Compiler *c, const char *name, int length) {
	if (c->enclosing == NULL) {
		return -1;
	}

	int local = resolve_local(c->enclosing, name, length);
	if (local >= 0) {
		c->enclosing->locals[local].is_captured = true;
		return add_upvalue(c, name, length, (uint8_t)local, true);
	}

	int upvalue = resolve_upvalue(c->enclosing, name, length);
	if (upvalue >= 0) {
		return add_upvalue(c, name, length, (uint8_t)upvalue, false);
	}

	return -1;
}

static void emit_local(Compiler *c, OpCode op, int slot, int line) {
	emit_bytes(c, (uint8_t)op, (uint8_t)slot, line);
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

static void compile_logical(Compiler *c, Node *node) {
	if (node->as.logical.op == TOK_AND) {
		compile_expression(c, node->as.logical.left);
		int end_jump = emit_jump(c, OP_JUMP_IF_FALSE, node->line);
		emit_byte(c, OP_POP, node->line);
		compile_expression(c, node->as.logical.right);
		patch_jump(c, end_jump, node->line);
		return;
	}

	if (node->as.logical.op == TOK_OR) {
		compile_expression(c, node->as.logical.left);
		int else_jump = emit_jump(c, OP_JUMP_IF_FALSE, node->line);
		int end_jump = emit_jump(c, OP_JUMP, node->line);
		patch_jump(c, else_jump, node->line);
		emit_byte(c, OP_POP, node->line);
		compile_expression(c, node->as.logical.right);
		patch_jump(c, end_jump, node->line);
		return;
	}

	compile_error(c, node->line, "Unsupported logical operator.");
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

static void compile_function_body(Compiler *c, Node *body) {
	for (int i = 0; i < body->as.program.count; i++) {
		compile_statement(c, body->as.program.items[i]);
		if (c->had_error) {
			return;
		}
	}

	emit_byte(c, OP_NIL, body->line);
	emit_byte(c, OP_RETURN, body->line);
}

static void emit_closure(Compiler *c, ObjFunction *function, Compiler *inner, int line) {
	int constant = make_constant(c, OBJ_VAL(function), line);
	emit_byte(c, OP_CLOSURE, line);
	emit_byte(c, (uint8_t)constant, line);
	emit_byte(c, (uint8_t)inner->upvalue_count, line);
	for (int i = 0; i < inner->upvalue_count; i++) {
		emit_byte(c, inner->upvalues[i].is_local ? 1 : 0, line);
		emit_byte(c, inner->upvalues[i].index, line);
	}
}

static void compile_function_decl_impl(Compiler *c, Node *node, bool as_statement) {
	ObjFunction *function = obj_function_new(node->as.fn_decl.name, node->as.fn_decl.param_count);
	Compiler inner;
	compiler_init(&inner, &function->chunk, true, c);

	for (int i = 0; i < node->as.fn_decl.param_count; i++) {
		declare_local(&inner, node->as.fn_decl.params[i], (int)strlen(node->as.fn_decl.params[i]));
	}

	compile_function_body(&inner, node->as.fn_decl.body);
	if (inner.had_error) {
		obj_free((Obj *)function);
		compiler_free(&inner);
		c->had_error = true;
		return;
	}

	emit_closure(c, function, &inner, node->line);
	if (as_statement && strlen(node->as.fn_decl.name) > 0) {
		if (c->scope_depth > 0) {
			declare_local(c, node->as.fn_decl.name, (int)strlen(node->as.fn_decl.name));
		} else {
			emit_global(c, OP_DEFINE_GLOBAL, node->as.fn_decl.name, node->line);
		}
	}

	compiler_free(&inner);
}

static void compile_function_decl(Compiler *c, Node *node) {
	compile_function_decl_impl(c, node, true);
}

static void compile_return(Compiler *c, Node *node) {
	if (!c->in_function) {
		compile_error(c, node->line, "return is only valid inside a function.");
		return;
	}

	if (node->as.return_stmt.value == NULL) {
		emit_byte(c, OP_NIL, node->line);
	} else {
		compile_expression(c, node->as.return_stmt.value);
	}
	emit_byte(c, OP_RETURN, node->line);
}

static void compile_assign(Compiler *c, Node *node) {
	compile_expression(c, node->as.assign.value);

	int local = resolve_local(c, node->as.assign.name, node->as.assign.length);
	if (local >= 0) {
		emit_local(c, OP_SET_LOCAL, local, node->line);
		return;
	}

	int upvalue = resolve_upvalue(c, node->as.assign.name, node->as.assign.length);
	if (upvalue >= 0) {
		emit_local(c, OP_SET_UPVALUE, upvalue, node->line);
		return;
	}

	emit_global(c, OP_SET_GLOBAL, node->as.assign.name, node->line);
}

static void compile_if(Compiler *c, Node *node) {
	compile_expression(c, node->as.if_stmt.condition);

	int then_jump = emit_jump(c, OP_JUMP_IF_FALSE, node->line);
	emit_byte(c, OP_POP, node->line);
	compile_statement(c, node->as.if_stmt.then_branch);

	int else_jump = emit_jump(c, OP_JUMP, node->line);
	patch_jump(c, then_jump, node->line);
	emit_byte(c, OP_POP, node->line);

	if (node->as.if_stmt.else_branch != NULL) {
		compile_statement(c, node->as.if_stmt.else_branch);
	}
	patch_jump(c, else_jump, node->line);
}

static void compile_while(Compiler *c, Node *node) {
	int loop_start = c->chunk->count;
	compile_expression(c, node->as.while_stmt.condition);

	int exit_jump = emit_jump(c, OP_JUMP_IF_FALSE, node->line);
	emit_byte(c, OP_POP, node->line);
	compile_statement(c, node->as.while_stmt.body);
	emit_loop(c, loop_start, node->line);

	patch_jump(c, exit_jump, node->line);
	emit_byte(c, OP_POP, node->line);
}

static void compile_for(Compiler *c, Node *node) {
	begin_scope(c);

	if (node->as.for_stmt.init != NULL) {
		compile_statement(c, node->as.for_stmt.init);
	}

	int loop_start = c->chunk->count;
	int exit_jump = -1;
	if (node->as.for_stmt.condition != NULL) {
		compile_expression(c, node->as.for_stmt.condition);
		exit_jump = emit_jump(c, OP_JUMP_IF_FALSE, node->line);
		emit_byte(c, OP_POP, node->line);
	}

	compile_statement(c, node->as.for_stmt.body);

	if (node->as.for_stmt.increment != NULL) {
		compile_expression(c, node->as.for_stmt.increment);
		emit_byte(c, OP_POP, node->line);
	}

	emit_loop(c, loop_start, node->line);

	if (exit_jump != -1) {
		patch_jump(c, exit_jump, node->line);
		emit_byte(c, OP_POP, node->line);
	}

	end_scope(c, node->line);
}

static void compile_block(Compiler *c, Node *node) {
	begin_scope(c);
	for (int i = 0; i < node->as.program.count; i++) {
		compile_statement(c, node->as.program.items[i]);
		if (c->had_error) {
			break;
		}
	}
	end_scope(c, node->line);
}

static void compile_array(Compiler *c, Node *node) {
	int count = node->as.array.count;
	Value *vals = NULL;
	if (count > 0) {
		vals = (Value *)malloc(sizeof(Value) * (size_t)count);
		if (vals == NULL) {
			compile_error(c, node->line, "Out of memory building array literal.");
			return;
		}
	}

	for (int i = 0; i < count; i++) {
		Node *elem = node->as.array.items[i];
		switch (elem->type) {
			case NODE_INT_LIT:
				vals[i] = NUMBER_VAL((double)elem->as.int_val);
				break;
			case NODE_FLOAT_LIT:
				vals[i] = NUMBER_VAL(elem->as.float_val);
				break;
			case NODE_STRING_LIT: {
				ObjString *s = obj_string_copy(elem->as.string_val.chars, elem->as.string_val.length);
				vals[i] = OBJ_VAL(s);
				break;
			}
			case NODE_BOOL_LIT:
				vals[i] = BOOL_VAL(elem->as.bool_val);
				break;
			case NODE_NIL_LIT:
				vals[i] = NIL_VAL;
				break;
			default:
				compile_error(c, elem->line, "Array element must be a literal in this stage.");
				free(vals);
				return;
		}
	}

	ObjArray *arr = obj_array_new(vals, count);
	free(vals);
	emit_constant(c, OBJ_VAL(arr), node->line);
}

static void compile_map(Compiler *c, Node *node) {
	ObjMap *map = obj_map_new();
	for (int i = 0; i < node->as.map.count; i++) {
		Node *key = node->as.map.keys[i];
		Node *value = node->as.map.values[i];
		if (key->type != NODE_STRING_LIT) {
			compile_error(c, key->line, "Map keys must be string literals in this stage.");
			obj_free((Obj *)map);
			return;
		}

		Value compiled_value = NIL_VAL;
		switch (value->type) {
			case NODE_INT_LIT:
				compiled_value = NUMBER_VAL((double)value->as.int_val);
				break;
			case NODE_FLOAT_LIT:
				compiled_value = NUMBER_VAL(value->as.float_val);
				break;
			case NODE_STRING_LIT: {
				ObjString *s = obj_string_copy(value->as.string_val.chars, value->as.string_val.length);
				compiled_value = OBJ_VAL(s);
				break;
			}
			case NODE_BOOL_LIT:
				compiled_value = BOOL_VAL(value->as.bool_val);
				break;
			case NODE_NIL_LIT:
				compiled_value = NIL_VAL;
				break;
			default:
				compile_error(c, value->line, "Map values must be literals in this stage.");
				obj_free((Obj *)map);
				return;
		}

		obj_map_set(map, key->as.string_val.chars, key->as.string_val.length, compiled_value);
	}

	emit_constant(c, OBJ_VAL(map), node->line);
}

static void compile_index(Compiler *c, Node *node) {
	compile_expression(c, node->as.index.left);
	compile_expression(c, node->as.index.index);
	emit_byte(c, OP_GET_INDEX, node->line);
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
			int local = resolve_local(c, node->as.ident.name, node->as.ident.length);
			if (local >= 0) {
				emit_local(c, OP_GET_LOCAL, local, node->line);
				break;
			}

			int upvalue = resolve_upvalue(c, node->as.ident.name, node->as.ident.length);
			if (upvalue >= 0) {
				emit_local(c, OP_GET_UPVALUE, upvalue, node->line);
				break;
			}

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
		case NODE_ARRAY:
			compile_array(c, node);
			break;
		case NODE_MAP:
			compile_map(c, node);
			break;
		case NODE_CALL:
			compile_call(c, node);
			break;
		case NODE_INDEX:
			compile_index(c, node);
			break;
		case NODE_BINARY:
			compile_binary(c, node);
			break;
		case NODE_LOGICAL:
			compile_logical(c, node);
			break;
		case NODE_ASSIGN:
			compile_assign(c, node);
			break;
		case NODE_FN_DECL:
			compile_function_decl_impl(c, node, false);
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
			if (c->scope_depth > 0) {
				declare_local(c, node->as.let_stmt.name, (int)strlen(node->as.let_stmt.name));
			} else {
				emit_global(c, OP_DEFINE_GLOBAL, node->as.let_stmt.name, node->line);
			}
			break;
		case NODE_FN_DECL:
			compile_function_decl(c, node);
			break;
		case NODE_RETURN:
			compile_return(c, node);
			break;
		case NODE_ASSIGN:
			compile_assign(c, node);
			emit_byte(c, OP_POP, node->line);
			break;
		case NODE_IF:
			compile_if(c, node);
			break;
		case NODE_WHILE:
			compile_while(c, node);
			break;
		case NODE_FOR:
			compile_for(c, node);
			break;
		case NODE_BLOCK:
			compile_block(c, node);
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
	compiler_init(&c, out_chunk, false, NULL);
	chunk_init(out_chunk);

	for (int i = 0; i < program->as.program.count; i++) {
		compile_statement(&c, program->as.program.items[i]);
	}

	emit_byte(&c, OP_RETURN, program->line);
	bool ok = !c.had_error;
	compiler_free(&c);
	return ok;
}
