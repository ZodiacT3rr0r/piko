#include "ast.h"

static void print_indent(int indent) {
	for (int i = 0; i < indent; i++) {
		printf(" ");
	}
}

char *node_strdup(const char *start, int length) {
	char *s = (char *)malloc((size_t)length + 1);
	if (s == NULL) {
		fprintf(stderr, "piko: out of memory\n");
		exit(1);
	}
	memcpy(s, start, (size_t)length);
	s[length] = '\0';
	return s;
}

Node *node_new(NodeType type, int line) {
	Node *n = (Node *)calloc(1, sizeof(Node));
	if (n == NULL) {
		fprintf(stderr, "piko: out of memory\n");
		exit(1);
	}
	n->type = type;
	n->line = line;
	return n;
}

Node *node_program(void) {
	Node *n = node_new(NODE_PROGRAM, 1);
	n->as.program.items = NULL;
	n->as.program.count = 0;
	return n;
}

void node_program_add(Node *program, Node *item) {
	int old_count = program->as.program.count;
	Node **grown = (Node **)realloc(program->as.program.items,
									(size_t)(old_count + 1) * sizeof(Node *));
	if (grown == NULL) {
		fprintf(stderr, "piko: out of memory\n");
		exit(1);
	}
	program->as.program.items = grown;
	program->as.program.items[old_count] = item;
	program->as.program.count = old_count + 1;
}

void node_free(Node *node) {
	if (node == NULL) {
		return;
	}

	switch (node->type) {
		case NODE_PROGRAM:
			for (int i = 0; i < node->as.program.count; i++) {
				node_free(node->as.program.items[i]);
			}
			free(node->as.program.items);
			break;
		case NODE_EXPR_STMT:
			node_free(node->as.expr_stmt.expr);
			break;
		case NODE_LET:
			free(node->as.let_stmt.name);
			node_free(node->as.let_stmt.init);
			break;
		case NODE_PRINT_STMT:
			node_free(node->as.print_stmt.expr);
			break;
		case NODE_STRING_LIT:
			free(node->as.string_val.chars);
			break;
		case NODE_IDENT:
			free(node->as.ident.name);
			break;
		case NODE_UNARY:
			node_free(node->as.unary.right);
			break;
		case NODE_CALL:
			node_free(node->as.call.callee);
			for (int i = 0; i < node->as.call.arg_count; i++) {
				node_free(node->as.call.args[i]);
			}
			free(node->as.call.args);
			break;
		case NODE_BINARY:
			node_free(node->as.binary.left);
			node_free(node->as.binary.right);
			break;
		case NODE_GROUPING:
			node_free(node->as.grouping.expr);
			break;
		case NODE_INT_LIT:
		case NODE_FLOAT_LIT:
		case NODE_BOOL_LIT:
		case NODE_NIL_LIT:
			break;
	}

	free(node);
}

const char *node_type_name(NodeType type) {
	switch (type) {
		case NODE_PROGRAM: return "PROGRAM";
		case NODE_EXPR_STMT: return "EXPR_STMT";
		case NODE_LET: return "LET";
		case NODE_PRINT_STMT: return "PRINT_STMT";
		case NODE_INT_LIT: return "INT_LIT";
		case NODE_FLOAT_LIT: return "FLOAT_LIT";
		case NODE_BOOL_LIT: return "BOOL_LIT";
		case NODE_NIL_LIT: return "NIL_LIT";
		case NODE_STRING_LIT: return "STRING_LIT";
		case NODE_IDENT: return "IDENT";
		case NODE_UNARY: return "UNARY";
		case NODE_CALL: return "CALL";
		case NODE_BINARY: return "BINARY";
		case NODE_GROUPING: return "GROUPING";
	}
	return "UNKNOWN";
}

static const char *token_op_name(TokenType type) {
	return token_type_name(type);
}

void node_print(Node *node, int indent) {
	if (node == NULL) {
		print_indent(indent);
		printf("<null>\n");
		return;
	}

	print_indent(indent);
	printf("%s", node_type_name(node->type));

	switch (node->type) {
		case NODE_PROGRAM:
			printf(" count=%d\n", node->as.program.count);
			for (int i = 0; i < node->as.program.count; i++) {
				node_print(node->as.program.items[i], indent + 2);
			}
			break;
		case NODE_EXPR_STMT:
			printf("\n");
			node_print(node->as.expr_stmt.expr, indent + 2);
			break;
		case NODE_LET:
			printf(" name=%s\n", node->as.let_stmt.name);
			node_print(node->as.let_stmt.init, indent + 2);
			break;
		case NODE_PRINT_STMT:
			printf("\n");
			node_print(node->as.print_stmt.expr, indent + 2);
			break;
		case NODE_INT_LIT:
			printf(" value=%lld\n", (long long)node->as.int_val);
			break;
		case NODE_FLOAT_LIT:
			printf(" value=%g\n", node->as.float_val);
			break;
		case NODE_BOOL_LIT:
			printf(" value=%s\n", node->as.bool_val ? "true" : "false");
			break;
		case NODE_NIL_LIT:
			printf("\n");
			break;
		case NODE_STRING_LIT:
			printf(" value=\"%s\"\n", node->as.string_val.chars);
			break;
		case NODE_IDENT:
			printf(" name=%s\n", node->as.ident.name);
			break;
		case NODE_UNARY:
			printf(" op=%s\n", token_op_name(node->as.unary.op));
			node_print(node->as.unary.right, indent + 2);
			break;
		case NODE_CALL:
			printf("\n");
			print_indent(indent + 2);
			printf("callee:\n");
			node_print(node->as.call.callee, indent + 4);
			print_indent(indent + 2);
			printf("args=%d\n", node->as.call.arg_count);
			for (int i = 0; i < node->as.call.arg_count; i++) {
				node_print(node->as.call.args[i], indent + 4);
			}
			break;
		case NODE_BINARY:
			printf(" op=%s\n", token_op_name(node->as.binary.op));
			node_print(node->as.binary.left, indent + 2);
			node_print(node->as.binary.right, indent + 2);
			break;
		case NODE_GROUPING:
			printf("\n");
			node_print(node->as.grouping.expr, indent + 2);
			break;
	}
}
