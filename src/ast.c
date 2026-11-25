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
	n->as.program.capacity = 0;
	return n;
}

void node_program_add(Node *program, Node *item) {
	if (program->as.program.count + 1 > program->as.program.capacity) {
		int new_cap = program->as.program.capacity < 8 ? 8 : program->as.program.capacity * 2;
		Node **grown = (Node **)realloc(program->as.program.items,
										(size_t)new_cap * sizeof(Node *));
		if (grown == NULL) {
			fprintf(stderr, "piko: out of memory\n");
			exit(1);
		}
		program->as.program.items = grown;
		program->as.program.capacity = new_cap;
	}
	program->as.program.items[program->as.program.count++] = item;
}

void node_free(Node *node) {
	if (node == NULL) {
		return;
	}

	switch (node->type) {
		case NODE_PROGRAM:
		case NODE_BLOCK:
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
		case NODE_FN_DECL:
			free(node->as.fn_decl.name);
			for (int i = 0; i < node->as.fn_decl.param_count; i++) {
				free(node->as.fn_decl.params[i]);
			}
			free(node->as.fn_decl.params);
			node_free(node->as.fn_decl.body);
			break;
		case NODE_RETURN:
			node_free(node->as.return_stmt.value);
			break;
		case NODE_PRINT_STMT:
			node_free(node->as.print_stmt.expr);
			break;
		case NODE_IF:
			node_free(node->as.if_stmt.condition);
			node_free(node->as.if_stmt.then_branch);
			node_free(node->as.if_stmt.else_branch);
			break;
		case NODE_WHILE:
			node_free(node->as.while_stmt.condition);
			node_free(node->as.while_stmt.body);
			break;
		case NODE_FOR:
			node_free(node->as.for_stmt.init);
			node_free(node->as.for_stmt.condition);
			node_free(node->as.for_stmt.increment);
			node_free(node->as.for_stmt.body);
			break;
		case NODE_FOR_IN:
			free(node->as.for_in_stmt.var_name);
			node_free(node->as.for_in_stmt.iterable);
			node_free(node->as.for_in_stmt.body);
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
		case NODE_ARRAY:
			for (int i = 0; i < node->as.array.count; i++) {
				node_free(node->as.array.items[i]);
			}
			free(node->as.array.items);
			break;
		case NODE_MAP:
			for (int i = 0; i < node->as.map.count; i++) {
				node_free(node->as.map.keys[i]);
				node_free(node->as.map.values[i]);
			}
			free(node->as.map.keys);
			free(node->as.map.values);
			break;
		case NODE_INDEX:
			node_free(node->as.index.left);
			node_free(node->as.index.index);
			break;
		case NODE_BINARY:
			node_free(node->as.binary.left);
			node_free(node->as.binary.right);
			break;
		case NODE_LOGICAL:
			node_free(node->as.logical.left);
			node_free(node->as.logical.right);
			break;
		case NODE_GROUPING:
			node_free(node->as.grouping.expr);
			break;
		case NODE_ASSIGN:
			free(node->as.assign.name);
			node_free(node->as.assign.value);
			break;
		case NODE_INDEX_ASSIGN:
			node_free(node->as.index_assign.target);
			node_free(node->as.index_assign.index);
			node_free(node->as.index_assign.value);
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
		case NODE_BLOCK: return "BLOCK";
		case NODE_EXPR_STMT: return "EXPR_STMT";
		case NODE_LET: return "LET";
		case NODE_PRINT_STMT: return "PRINT_STMT";
		case NODE_FN_DECL: return "FN_DECL";
		case NODE_RETURN: return "RETURN";
		case NODE_IF: return "IF";
		case NODE_WHILE: return "WHILE";
		case NODE_FOR: return "FOR";
		case NODE_FOR_IN: return "FOR_IN";
		case NODE_INT_LIT: return "INT_LIT";
		case NODE_FLOAT_LIT: return "FLOAT_LIT";
		case NODE_BOOL_LIT: return "BOOL_LIT";
		case NODE_NIL_LIT: return "NIL_LIT";
		case NODE_STRING_LIT: return "STRING_LIT";
		case NODE_IDENT: return "IDENT";
		case NODE_UNARY: return "UNARY";
		case NODE_CALL: return "CALL";
		case NODE_ARRAY: return "ARRAY";
		case NODE_MAP: return "MAP";
		case NODE_BINARY: return "BINARY";
		case NODE_LOGICAL: return "LOGICAL";
		case NODE_INDEX: return "INDEX";
		case NODE_GROUPING: return "GROUPING";
		case NODE_ASSIGN: return "ASSIGN";
		case NODE_INDEX_ASSIGN: return "INDEX_ASSIGN";
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
		case NODE_BLOCK:
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
		case NODE_FN_DECL:
			printf(" name=%s params=%d\n", node->as.fn_decl.name, node->as.fn_decl.param_count);
			for (int i = 0; i < node->as.fn_decl.param_count; i++) {
				print_indent(indent + 2);
				printf("param=%s\n", node->as.fn_decl.params[i]);
			}
			print_indent(indent + 2);
			printf("body:\n");
			node_print(node->as.fn_decl.body, indent + 4);
			break;
		case NODE_RETURN:
			printf("\n");
			if (node->as.return_stmt.value != NULL) {
				node_print(node->as.return_stmt.value, indent + 2);
			}
			break;
		case NODE_IF:
			printf("\n");
			print_indent(indent + 2);
			printf("condition:\n");
			node_print(node->as.if_stmt.condition, indent + 4);
			print_indent(indent + 2);
			printf("then:\n");
			node_print(node->as.if_stmt.then_branch, indent + 4);
			if (node->as.if_stmt.else_branch != NULL) {
				print_indent(indent + 2);
				printf("else:\n");
				node_print(node->as.if_stmt.else_branch, indent + 4);
			}
			break;
		case NODE_WHILE:
			printf("\n");
			print_indent(indent + 2);
			printf("condition:\n");
			node_print(node->as.while_stmt.condition, indent + 4);
			print_indent(indent + 2);
			printf("body:\n");
			node_print(node->as.while_stmt.body, indent + 4);
			break;
		case NODE_FOR:
			printf("\n");
			if (node->as.for_stmt.init != NULL) {
				print_indent(indent + 2);
				printf("init:\n");
				node_print(node->as.for_stmt.init, indent + 4);
			}
			if (node->as.for_stmt.condition != NULL) {
				print_indent(indent + 2);
				printf("condition:\n");
				node_print(node->as.for_stmt.condition, indent + 4);
			}
			if (node->as.for_stmt.increment != NULL) {
				print_indent(indent + 2);
				printf("increment:\n");
				node_print(node->as.for_stmt.increment, indent + 4);
			}
			print_indent(indent + 2);
			printf("body:\n");
			node_print(node->as.for_stmt.body, indent + 4);
			break;
		case NODE_FOR_IN:
			printf(" var=%s\n", node->as.for_in_stmt.var_name);
			print_indent(indent + 2);
			printf("iterable:\n");
			node_print(node->as.for_in_stmt.iterable, indent + 4);
			print_indent(indent + 2);
			printf("body:\n");
			node_print(node->as.for_in_stmt.body, indent + 4);
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
		case NODE_ARRAY:
			printf("\n");
			print_indent(indent + 2);
			printf("items=%d\n", node->as.array.count);
			for (int i = 0; i < node->as.array.count; i++) {
				node_print(node->as.array.items[i], indent + 4);
			}
			break;
		case NODE_MAP:
			printf("\n");
			print_indent(indent + 2);
			printf("pairs=%d\n", node->as.map.count);
			for (int i = 0; i < node->as.map.count; i++) {
				print_indent(indent + 4);
				printf("key:\n");
				node_print(node->as.map.keys[i], indent + 6);
				print_indent(indent + 4);
				printf("value:\n");
				node_print(node->as.map.values[i], indent + 6);
			}
			break;
		case NODE_INDEX:
			printf("\n");
			print_indent(indent + 2);
			printf("left:\n");
			node_print(node->as.index.left, indent + 4);
			print_indent(indent + 2);
			printf("index:\n");
			node_print(node->as.index.index, indent + 4);
			break;
		case NODE_BINARY:
			printf(" op=%s\n", token_op_name(node->as.binary.op));
			node_print(node->as.binary.left, indent + 2);
			node_print(node->as.binary.right, indent + 2);
			break;
		case NODE_LOGICAL:
			printf(" op=%s\n", token_op_name(node->as.logical.op));
			node_print(node->as.logical.left, indent + 2);
			node_print(node->as.logical.right, indent + 2);
			break;
		case NODE_GROUPING:
			printf("\n");
			node_print(node->as.grouping.expr, indent + 2);
			break;
		case NODE_ASSIGN:
			printf(" name=%s\n", node->as.assign.name);
			node_print(node->as.assign.value, indent + 2);
			break;
		case NODE_INDEX_ASSIGN:
			printf("\n");
			print_indent(indent + 2);
			printf("target:\n");
			node_print(node->as.index_assign.target, indent + 4);
			print_indent(indent + 2);
			printf("index:\n");
			node_print(node->as.index_assign.index, indent + 4);
			print_indent(indent + 2);
			printf("value:\n");
			node_print(node->as.index_assign.value, indent + 4);
			break;
	}
}
