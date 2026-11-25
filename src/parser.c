#include "parser.h"

static void parser_advance(Parser *p);
static bool parser_match(Parser *p, TokenType type);
static bool parser_check(Parser *p, TokenType type);
static void parser_consume(Parser *p, TokenType type, const char *message);
static void parser_error_at_current(Parser *p, const char *message);
static void parser_synchronize(Parser *p);

static Node *parse_declaration(Parser *p);
static Node *parse_statement(Parser *p);
static Node *parse_let_declaration(Parser *p);
static Node *parse_fn_declaration(Parser *p);
static Node *parse_if_statement(Parser *p);
static Node *parse_while_statement(Parser *p);
static Node *parse_for_statement(Parser *p);
static Node *parse_block_statement(Parser *p);
static Node *parse_print_statement(Parser *p);
static Node *parse_return_statement(Parser *p);
static Node *parse_expression_statement(Parser *p);

static Node *parse_expression(Parser *p);
static Node *parse_assignment(Parser *p);
static Node *parse_logic_or(Parser *p);
static Node *parse_logic_and(Parser *p);
static Node *parse_equality(Parser *p);
static Node *parse_comparison(Parser *p);
static Node *parse_term(Parser *p);
static Node *parse_factor(Parser *p);
static Node *parse_unary(Parser *p);
static Node *parse_call(Parser *p);
static Node *parse_primary(Parser *p);
static Node *parse_block_program(Parser *p);

static void parser_error_at(Parser *p, Token token, const char *message) {
	if (p->panic_mode) {
		return;
	}

	p->panic_mode = true;
	p->had_error = true;

	fprintf(stderr, "[line %d] Error", token.line);
	if (token.type == TOK_EOF) {
		fprintf(stderr, " at end");
	} else if (token.type != TOK_ERROR) {
		fprintf(stderr, " at '%.*s'", token.length, token.start);
	}

	fprintf(stderr, ": %s\n", message);
}

static void parser_error_at_current(Parser *p, const char *message) {
	parser_error_at(p, p->current, message);
}

void parser_init(Parser *p, const char *source) {
	lexer_init(&p->lexer, source);
	p->had_error = false;
	p->panic_mode = false;

	p->current.type = TOK_ERROR;
	p->current.start = "";
	p->current.length = 0;
	p->current.line = 1;

	p->previous = p->current;
	parser_advance(p);
}

static void parser_advance(Parser *p) {
	p->previous = p->current;

	for (;;) {
		p->current = lexer_next(&p->lexer);
		if (p->current.type != TOK_ERROR) {
			break;
		}
		parser_error_at_current(p, p->current.start);
	}
}

static bool parser_check(Parser *p, TokenType type) {
	return p->current.type == type;
}

static bool parser_match(Parser *p, TokenType type) {
	if (!parser_check(p, type)) {
		return false;
	}
	parser_advance(p);
	return true;
}

static void parser_consume(Parser *p, TokenType type, const char *message) {
	if (p->current.type == type) {
		parser_advance(p);
		return;
	}
	parser_error_at_current(p, message);
}

static void parser_synchronize(Parser *p) {
	p->panic_mode = false;

	while (p->current.type != TOK_EOF) {
		if (p->previous.type == TOK_SEMICOLON) {
			return;
		}

		switch (p->current.type) {
			case TOK_LET:
			case TOK_FN:
			case TOK_IF:
			case TOK_WHILE:
			case TOK_FOR:
			case TOK_RETURN:
			case TOK_PRINT:
				return;
			default:
				break;
		}

		parser_advance(p);
	}
}

static Node *new_literal_int(Token token) {
	Node *n = node_new(NODE_INT_LIT, token.line);
	n->as.int_val = strtoll(token.start, NULL, 10);
	return n;
}

static Node *new_literal_float(Token token) {
	Node *n = node_new(NODE_FLOAT_LIT, token.line);
	n->as.float_val = strtod(token.start, NULL);
	return n;
}

static Node *new_literal_string(Token token) {
	Node *n = node_new(NODE_STRING_LIT, token.line);
	n->as.string_val.chars = node_strdup(token.start, token.length);
	n->as.string_val.length = token.length;
	return n;
}

static Node *new_ident(Token token) {
	Node *n = node_new(NODE_IDENT, token.line);
	n->as.ident.name = node_strdup(token.start, token.length);
	n->as.ident.length = token.length;
	return n;
}

static Node *new_unary(TokenType op, Node *right, int line) {
	Node *n = node_new(NODE_UNARY, line);
	n->as.unary.op = op;
	n->as.unary.right = right;
	return n;
}

static Node *new_call(Node *callee, Node **args, int arg_count, int line) {
	Node *n = node_new(NODE_CALL, line);
	n->as.call.callee = callee;
	n->as.call.args = args;
	n->as.call.arg_count = arg_count;
	return n;
}

static Node *new_fn_decl(Token name, char **params, int param_count, Node *body) {
	Node *n = node_new(NODE_FN_DECL, name.line);
	n->as.fn_decl.name = node_strdup(name.start, name.length);
	n->as.fn_decl.params = params;
	n->as.fn_decl.param_count = param_count;
	n->as.fn_decl.body = body;
	return n;
}

static Node *new_return(Node *value, int line) {
	Node *n = node_new(NODE_RETURN, line);
	n->as.return_stmt.value = value;
	return n;
}

static Node *new_binary(Node *left, TokenType op, Node *right, int line) {
	Node *n = node_new(NODE_BINARY, line);
	n->as.binary.left = left;
	n->as.binary.op = op;
	n->as.binary.right = right;
	return n;
}

static Node *new_logical(Node *left, TokenType op, Node *right, int line) {
	Node *n = node_new(NODE_LOGICAL, line);
	n->as.logical.left = left;
	n->as.logical.op = op;
	n->as.logical.right = right;
	return n;
}

static Node *parse_primary(Parser *p) {
	if (parser_match(p, TOK_FALSE)) {
		Node *n = node_new(NODE_BOOL_LIT, p->previous.line);
		n->as.bool_val = false;
		return n;
	}
	if (parser_match(p, TOK_TRUE)) {
		Node *n = node_new(NODE_BOOL_LIT, p->previous.line);
		n->as.bool_val = true;
		return n;
	}
	if (parser_match(p, TOK_NIL)) {
		return node_new(NODE_NIL_LIT, p->previous.line);
	}
	if (parser_match(p, TOK_INT)) {
		return new_literal_int(p->previous);
	}
	if (parser_match(p, TOK_LBRACKET)) {
		int count = 0;
		Node **items = NULL;
		if (!parser_check(p, TOK_RBRACKET)) {
			int capacity = 0;
			for (;;) {
				if (count + 1 > capacity) {
					capacity = capacity < 4 ? 4 : capacity * 2;
					items = (Node **)realloc(items, sizeof(Node *) * (size_t)capacity);
					if (items == NULL) {
						fprintf(stderr, "piko: out of memory\n");
						exit(1);
					}
				}

				items[count++] = parse_expression(p);
				if (!parser_match(p, TOK_COMMA)) {
					break;
				}
			}
		}

		parser_consume(p, TOK_RBRACKET, "Expect ']' after array literal.");
		Node *n = node_new(NODE_ARRAY, p->previous.line);
		n->as.array.items = items;
		n->as.array.count = count;
		return n;
	}
	if (parser_match(p, TOK_LBRACE)) {
		int count = 0;
		Node **keys = NULL;
		Node **values = NULL;
		if (!parser_check(p, TOK_RBRACE)) {
			int capacity = 0;
			for (;;) {
				if (count + 1 > capacity) {
					capacity = capacity < 4 ? 4 : capacity * 2;
					keys = (Node **)realloc(keys, sizeof(Node *) * (size_t)capacity);
					values = (Node **)realloc(values, sizeof(Node *) * (size_t)capacity);
					if (keys == NULL || values == NULL) {
						fprintf(stderr, "piko: out of memory\n");
						exit(1);
					}
				}

				if (!parser_check(p, TOK_STRING) && !parser_check(p, TOK_FSTRING)) {
					parser_error_at_current(p, "Expect string key in map literal.");
					break;
				}
				keys[count] = new_literal_string(p->current);
				parser_advance(p);
				parser_consume(p, TOK_COLON, "Expect ':' after map key.");
				values[count] = parse_expression(p);
				count++;
				if (!parser_match(p, TOK_COMMA)) {
					break;
				}
			}
		}

		parser_consume(p, TOK_RBRACE, "Expect '}' after map literal.");
		Node *n = node_new(NODE_MAP, p->previous.line);
		n->as.map.keys = keys;
		n->as.map.values = values;
		n->as.map.count = count;
		return n;
	}
	if (parser_match(p, TOK_FLOAT)) {
		return new_literal_float(p->previous);
	}
	if (parser_match(p, TOK_STRING) || parser_match(p, TOK_FSTRING)) {
		return new_literal_string(p->previous);
	}
	if (parser_match(p, TOK_IDENT)) {
		return new_ident(p->previous);
	}

	if (parser_match(p, TOK_FN)) {
		int line = p->previous.line;

		char *name_str = NULL;
		if (parser_check(p, TOK_IDENT)) {
			parser_advance(p);
			name_str = node_strdup(p->previous.start, p->previous.length);
		} else {
			name_str = node_strdup("", 0);
		}

		parser_consume(p, TOK_LPAREN, "Expect '(' after function name.");

		char **params = NULL;
		int param_count = 0;
		int capacity = 0;
		if (!parser_check(p, TOK_RPAREN)) {
			for (;;) {
				parser_consume(p, TOK_IDENT, "Expect parameter name.");
				Token param = p->previous;
				if (param_count + 1 > capacity) {
					capacity = capacity < 4 ? 4 : capacity * 2;
					params = (char **)realloc(params, sizeof(char *) * (size_t)capacity);
					if (params == NULL) {
						fprintf(stderr, "piko: out of memory\n");
						exit(1);
					}
				}
				params[param_count++] = node_strdup(param.start, param.length);
				if (!parser_match(p, TOK_COMMA)) {
					break;
				}
			}
		}

		parser_consume(p, TOK_RPAREN, "Expect ')' after parameters.");
		parser_consume(p, TOK_LBRACE, "Expect '{' before function body.");

		Node *body = parse_block_program(p);
		parser_consume(p, TOK_RBRACE, "Expect '}' after function body.");

		Node *n = node_new(NODE_FN_DECL, line);
		n->as.fn_decl.name = name_str;
		n->as.fn_decl.params = params;
		n->as.fn_decl.param_count = param_count;
		n->as.fn_decl.body = body;
		return n;
	}
	if (parser_match(p, TOK_LPAREN)) {
		int line = p->previous.line;
		Node *expr = parse_expression(p);
		parser_consume(p, TOK_RPAREN, "Expect ')' after expression.");
		Node *n = node_new(NODE_GROUPING, line);
		n->as.grouping.expr = expr;
		return n;
	}

	parser_error_at_current(p, "Expect expression.");
	return node_new(NODE_NIL_LIT, p->current.line);
}

static Node *parse_block_program(Parser *p) {
	Node *block = node_program();

	while (!parser_check(p, TOK_RBRACE) && !parser_check(p, TOK_EOF)) {
		Node *decl = parse_declaration(p);
		node_program_add(block, decl);

		if (p->panic_mode) {
			parser_synchronize(p);
		}
	}

	return block;
}

static Node *parse_block_statement(Parser *p) {
	int line = p->previous.line;
	Node *block = node_new(NODE_BLOCK, line);
	block->as.program.items = NULL;
	block->as.program.count = 0;

	while (!parser_check(p, TOK_RBRACE) && !parser_check(p, TOK_EOF)) {
		Node *decl = parse_declaration(p);
		node_program_add(block, decl);

		if (p->panic_mode) {
			parser_synchronize(p);
		}
	}

	parser_consume(p, TOK_RBRACE, "Expect '}' after block.");
	return block;
}

static Node *parse_fn_declaration(Parser *p) {
	parser_consume(p, TOK_IDENT, "Expect function name after 'fn'.");
	Token name = p->previous;

	parser_consume(p, TOK_LPAREN, "Expect '(' after function name.");

	char **params = NULL;
	int param_count = 0;
	int capacity = 0;
	if (!parser_check(p, TOK_RPAREN)) {
		for (;;) {
			parser_consume(p, TOK_IDENT, "Expect parameter name.");
			Token param = p->previous;
			if (param_count + 1 > capacity) {
				capacity = capacity < 4 ? 4 : capacity * 2;
				params = (char **)realloc(params, sizeof(char *) * (size_t)capacity);
				if (params == NULL) {
					fprintf(stderr, "piko: out of memory\n");
					exit(1);
				}
			}
			params[param_count++] = node_strdup(param.start, param.length);
			if (!parser_match(p, TOK_COMMA)) {
				break;
			}
		}
	}

	parser_consume(p, TOK_RPAREN, "Expect ')' after parameters.");
	parser_consume(p, TOK_LBRACE, "Expect '{' before function body.");

	Node *body = parse_block_program(p);
	parser_consume(p, TOK_RBRACE, "Expect '}' after function body.");

	return new_fn_decl(name, params, param_count, body);
}

static Node *parse_unary(Parser *p) {
	if (parser_match(p, TOK_BANG) || parser_match(p, TOK_MINUS)) {
		Token op = p->previous;
		Node *right = parse_unary(p);
		return new_unary(op.type, right, op.line);
	}
	return parse_call(p);
}

static Node *parse_call(Parser *p) {
	Node *expr = parse_primary(p);

	for (;;) {
		if (parser_match(p, TOK_LPAREN)) {
			int line = p->previous.line;
			int arg_count = 0;
			Node **args = NULL;
			if (!parser_check(p, TOK_RPAREN)) {
				int capacity = 0;
				for (;;) {
					if (arg_count + 1 > capacity) {
						capacity = capacity < 4 ? 4 : capacity * 2;
						args = (Node **)realloc(args, sizeof(Node *) * (size_t)capacity);
						if (args == NULL) {
							fprintf(stderr, "piko: out of memory\n");
							exit(1);
						}
					}

					args[arg_count++] = parse_expression(p);
					if (!parser_match(p, TOK_COMMA)) {
						break;
					}
				}
			}

			parser_consume(p, TOK_RPAREN, "Expect ')' after arguments.");
			expr = new_call(expr, args, arg_count, line);
		} else if (parser_match(p, TOK_LBRACKET)) {
			int idx_line = p->previous.line;
			Node *index = parse_expression(p);
			parser_consume(p, TOK_RBRACKET, "Expect ']' after index.");

			Node *n = node_new(NODE_INDEX, idx_line);
			n->as.index.left = expr;
			n->as.index.index = index;
			expr = n;
		} else {
			break;
		}
	}

	return expr;
}

static Node *parse_factor(Parser *p) {
	Node *expr = parse_unary(p);

	while (parser_match(p, TOK_STAR) || parser_match(p, TOK_SLASH) || parser_match(p, TOK_PERCENT)) {
		Token op = p->previous;
		Node *right = parse_unary(p);
		expr = new_binary(expr, op.type, right, op.line);
	}

	return expr;
}

static Node *parse_term(Parser *p) {
	Node *expr = parse_factor(p);

	while (parser_match(p, TOK_PLUS) || parser_match(p, TOK_MINUS)) {
		Token op = p->previous;
		Node *right = parse_factor(p);
		expr = new_binary(expr, op.type, right, op.line);
	}

	return expr;
}

static Node *parse_comparison(Parser *p) {
	Node *expr = parse_term(p);

	while (parser_match(p, TOK_GT) || parser_match(p, TOK_GT_EQ) ||
		   parser_match(p, TOK_LT) || parser_match(p, TOK_LT_EQ)) {
		Token op = p->previous;
		Node *right = parse_term(p);
		expr = new_binary(expr, op.type, right, op.line);
	}

	return expr;
}

static Node *parse_equality(Parser *p) {
	Node *expr = parse_comparison(p);

	while (parser_match(p, TOK_EQ_EQ) || parser_match(p, TOK_BANG_EQ)) {
		Token op = p->previous;
		Node *right = parse_comparison(p);
		expr = new_binary(expr, op.type, right, op.line);
	}

	return expr;
}

static Node *parse_logic_and(Parser *p) {
	Node *expr = parse_equality(p);

	while (parser_match(p, TOK_AND)) {
		Token op = p->previous;
		Node *right = parse_equality(p);
		expr = new_logical(expr, op.type, right, op.line);
	}

	return expr;
}

static Node *parse_logic_or(Parser *p) {
	Node *expr = parse_logic_and(p);

	while (parser_match(p, TOK_OR)) {
		Token op = p->previous;
		Node *right = parse_logic_and(p);
		expr = new_logical(expr, op.type, right, op.line);
	}

	return expr;
}

static Node *parse_assignment(Parser *p) {
	Node *expr = parse_logic_or(p);
	if (parser_match(p, TOK_EQ)) {
		Token equals = p->previous;
		if (expr->type != NODE_IDENT) {
			parser_error_at(p, equals, "Invalid assignment target.");
			return expr;
		}

		Token name;
		name.line = expr->line;
		name.start = expr->as.ident.name;
		name.length = expr->as.ident.length;
		Node *value = parse_assignment(p);
		char *name_copy = node_strdup(name.start, name.length);
		Node *assign = node_new(NODE_ASSIGN, name.line);
		assign->as.assign.name = name_copy;
		assign->as.assign.length = name.length;
		assign->as.assign.value = value;
		expr->as.ident.name = NULL;
		node_free(expr);
		return assign;
	}

	return expr;
}

static Node *parse_expression(Parser *p) {
	return parse_assignment(p);
}

static Node *parse_let_declaration(Parser *p) {
	parser_consume(p, TOK_IDENT, "Expect variable name after 'let'.");
	Token name = p->previous;

	Node *init = NULL;
	if (parser_match(p, TOK_EQ)) {
		init = parse_expression(p);
	} else {
		init = node_new(NODE_NIL_LIT, name.line);
	}
	parser_consume(p, TOK_SEMICOLON, "Expect ';' after let declaration.");

	Node *n = node_new(NODE_LET, name.line);
	n->as.let_stmt.name = node_strdup(name.start, name.length);
	n->as.let_stmt.init = init;
	return n;
}

static Node *parse_if_statement(Parser *p) {
	int line = p->previous.line;
	Node *condition = parse_expression(p);
	Node *then_branch = parse_statement(p);
	Node *else_branch = NULL;
	if (parser_match(p, TOK_ELSE)) {
		else_branch = parse_statement(p);
	}

	Node *n = node_new(NODE_IF, line);
	n->as.if_stmt.condition = condition;
	n->as.if_stmt.then_branch = then_branch;
	n->as.if_stmt.else_branch = else_branch;
	return n;
}

static Node *parse_while_statement(Parser *p) {
	int line = p->previous.line;
	Node *condition = parse_expression(p);
	Node *body = parse_statement(p);

	Node *n = node_new(NODE_WHILE, line);
	n->as.while_stmt.condition = condition;
	n->as.while_stmt.body = body;
	return n;
}

static Node *parse_for_statement(Parser *p) {
	int line = p->previous.line;
	parser_consume(p, TOK_LPAREN, "Expect '(' after 'for'.");

	Node *init = NULL;
	if (parser_match(p, TOK_SEMICOLON)) {
		init = NULL;
	} else if (parser_match(p, TOK_LET)) {
		init = parse_let_declaration(p);
	} else {
		init = parse_expression_statement(p);
	}

	Node *condition = NULL;
	if (!parser_check(p, TOK_SEMICOLON)) {
		condition = parse_expression(p);
	}
	parser_consume(p, TOK_SEMICOLON, "Expect ';' after loop condition.");

	Node *increment = NULL;
	if (!parser_check(p, TOK_RPAREN)) {
		increment = parse_expression(p);
	}
	parser_consume(p, TOK_RPAREN, "Expect ')' after for clauses.");

	Node *body = parse_statement(p);

	Node *n = node_new(NODE_FOR, line);
	n->as.for_stmt.init = init;
	n->as.for_stmt.condition = condition;
	n->as.for_stmt.increment = increment;
	n->as.for_stmt.body = body;
	return n;
}

static Node *parse_return_statement(Parser *p) {
	int line = p->previous.line;
	Node *value = NULL;
	if (!parser_check(p, TOK_SEMICOLON)) {
		value = parse_expression(p);
	}
	parser_consume(p, TOK_SEMICOLON, "Expect ';' after return statement.");
	return new_return(value, line);
}

static Node *parse_print_statement(Parser *p) {
	int line = p->previous.line;

	parser_consume(p, TOK_LPAREN, "Expect '(' after 'print'.");
	Node *expr = parse_expression(p);
	parser_consume(p, TOK_RPAREN, "Expect ')' after print argument.");
	parser_consume(p, TOK_SEMICOLON, "Expect ';' after print statement.");

	Node *n = node_new(NODE_PRINT_STMT, line);
	n->as.print_stmt.expr = expr;
	return n;
}

static Node *parse_expression_statement(Parser *p) {
	Node *expr = parse_expression(p);
	parser_consume(p, TOK_SEMICOLON, "Expect ';' after expression.");

	Node *n = node_new(NODE_EXPR_STMT, expr->line);
	n->as.expr_stmt.expr = expr;
	return n;
}

static Node *parse_statement(Parser *p) {
	if (parser_match(p, TOK_IF)) {
		return parse_if_statement(p);
	}
	if (parser_match(p, TOK_WHILE)) {
		return parse_while_statement(p);
	}
	if (parser_match(p, TOK_FOR)) {
		return parse_for_statement(p);
	}
	if (parser_match(p, TOK_LBRACE)) {
		return parse_block_statement(p);
	}
	if (parser_match(p, TOK_PRINT)) {
		return parse_print_statement(p);
	}
	if (parser_match(p, TOK_RETURN)) {
		return parse_return_statement(p);
	}
	return parse_expression_statement(p);
}

static Node *parse_declaration(Parser *p) {
	if (parser_match(p, TOK_FN)) {
		return parse_fn_declaration(p);
	}
	if (parser_match(p, TOK_LET)) {
		return parse_let_declaration(p);
	}

	return parse_statement(p);
}

Node *parse_program(Parser *p) {
	Node *program = node_program();

	while (!parser_check(p, TOK_EOF)) {
		Node *decl = parse_declaration(p);
		node_program_add(program, decl);

		if (p->panic_mode) {
			parser_synchronize(p);
		}
	}

	return program;
}

Node *parse_source(const char *source, bool *had_error) {
	Parser p;
	parser_init(&p, source);

	Node *program = parse_program(&p);

	if (had_error != NULL) {
		*had_error = p.had_error;
	}

	return program;
}
