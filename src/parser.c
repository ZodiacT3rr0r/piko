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
static Node *parse_print_statement(Parser *p);
static Node *parse_expression_statement(Parser *p);

static Node *parse_expression(Parser *p);
static Node *parse_equality(Parser *p);
static Node *parse_comparison(Parser *p);
static Node *parse_term(Parser *p);
static Node *parse_factor(Parser *p);
static Node *parse_unary(Parser *p);
static Node *parse_call(Parser *p);
static Node *parse_primary(Parser *p);

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
			case TOK_IF:
			case TOK_WHILE:
			case TOK_FOR:
			case TOK_FN:
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

static Node *new_binary(Node *left, TokenType op, Node *right, int line) {
	Node *n = node_new(NODE_BINARY, line);
	n->as.binary.left = left;
	n->as.binary.op = op;
	n->as.binary.right = right;
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
	if (parser_match(p, TOK_FLOAT)) {
		return new_literal_float(p->previous);
	}
	if (parser_match(p, TOK_STRING) || parser_match(p, TOK_FSTRING)) {
		return new_literal_string(p->previous);
	}
	if (parser_match(p, TOK_IDENT)) {
		return new_ident(p->previous);
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
		if (!parser_match(p, TOK_LPAREN)) {
			break;
		}

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

static Node *parse_expression(Parser *p) {
	return parse_equality(p);
}

static Node *parse_let_declaration(Parser *p) {
	parser_consume(p, TOK_IDENT, "Expect variable name after 'let'.");
	Token name = p->previous;

	parser_consume(p, TOK_EQ, "Expect '=' after variable name.");
	Node *init = parse_expression(p);
	parser_consume(p, TOK_SEMICOLON, "Expect ';' after let declaration.");

	Node *n = node_new(NODE_LET, name.line);
	n->as.let_stmt.name = node_strdup(name.start, name.length);
	n->as.let_stmt.init = init;
	return n;
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
	if (parser_match(p, TOK_PRINT)) {
		return parse_print_statement(p);
	}
	return parse_expression_statement(p);
}

static Node *parse_declaration(Parser *p) {
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
