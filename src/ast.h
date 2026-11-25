#ifndef PIKO_AST_H
#define PIKO_AST_H

#include "common.h"
#include "lexer.h"

typedef enum {
	NODE_PROGRAM,
	NODE_BLOCK,
	NODE_EXPR_STMT,
	NODE_LET,
	NODE_PRINT_STMT,
	NODE_FN_DECL,
	NODE_RETURN,
	NODE_IF,
	NODE_WHILE,
	NODE_FOR,
	NODE_FOR_IN,

	NODE_INT_LIT,
	NODE_FLOAT_LIT,
	NODE_BOOL_LIT,
	NODE_NIL_LIT,
	NODE_STRING_LIT,
	NODE_IDENT,
	NODE_UNARY,
	NODE_CALL,
	NODE_ARRAY,
	NODE_MAP,
	NODE_BINARY,
	NODE_LOGICAL,
	NODE_INDEX,
	NODE_GROUPING,
	NODE_ASSIGN,
	NODE_INDEX_ASSIGN,
} NodeType;

typedef struct Node Node;

struct Node {
	NodeType type;
	int line;

	union {
		struct {
			Node **items;
			int count;
			int capacity;
		} program;

		struct {
			Node *expr;
		} expr_stmt;

		struct {
			char *name;
			Node *init;
		} let_stmt;

		struct {
			char *name;
			char **params;
			int param_count;
			Node *body;
		} fn_decl;

		struct {
			Node *value;
		} return_stmt;

		struct {
			Node *expr;
		} print_stmt;

		struct {
			Node *condition;
			Node *then_branch;
			Node *else_branch;
		} if_stmt;

		struct {
			Node *condition;
			Node *body;
		} while_stmt;

		struct {
			Node *init;
			Node *condition;
			Node *increment;
			Node *body;
		} for_stmt;

		struct {
			char *var_name;
			int var_length;
			Node *iterable;
			Node *body;
		} for_in_stmt;

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
			Node *callee;
			Node **args;
			int arg_count;
		} call;

		struct {
			Node **items;
			int count;
		} array;

		struct {
			Node **keys;
			Node **values;
			int count;
		} map;

		struct {
			Node *left;
			Node *index;
		} index;

		struct {
			TokenType op;
			Node *left;
			Node *right;
		} binary;

		struct {
			TokenType op;
			Node *left;
			Node *right;
		} logical;

		struct {
			Node *expr;
		} grouping;

		struct {
			char *name;
			int length;
			Node *value;
		} assign;

		struct {
			Node *target;
			Node *index;
			Node *value;
		} index_assign;
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
