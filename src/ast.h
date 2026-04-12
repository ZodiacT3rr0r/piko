#ifndef PIKO_AST_H
#define PIKO_AST_H

#include "common.h"
#include "lexer.h"

typedef enum {
	NODE_PROGRAM,
	NODE_EXPR_STMT,
	NODE_LET,
	NODE_PRINT_STMT,

	NODE_INT_LIT,
	NODE_FLOAT_LIT,
	NODE_BOOL_LIT,
	NODE_NIL_LIT,
	NODE_STRING_LIT,
	NODE_IDENT,
	NODE_UNARY,
	NODE_BINARY,
	NODE_GROUPING,
} NodeType;

typedef struct Node Node;

struct Node {
	NodeType type;
	int line;

	union {
		struct {
			Node **items;
			int count;
		} program;

		struct {
			Node *expr;
		} expr_stmt;

		struct {
			char *name;
			Node *init;
		} let_stmt;

		struct {
			Node *expr;
		} print_stmt;

		int64_t int_val;
		double float_val;
		bool bool_val;

		struct {
			char *chars;
			int length;
		} string_val;

		struct {
			char *name;
			int length;
		} ident;

		struct {
			TokenType op;
			Node *right;
		} unary;

		struct {
			TokenType op;
			Node *left;
			Node *right;
		} binary;

		struct {
			Node *expr;
		} grouping;
	} as;
};

Node *node_new(NodeType type, int line);
Node *node_program(void);
void node_program_add(Node *program, Node *item);
void node_free(Node *node);
void node_print(Node *node, int indent);

char *node_strdup(const char *start, int length);
const char *node_type_name(NodeType type);

#endif
