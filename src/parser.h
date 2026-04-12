#ifndef PIKO_PARSER_H
#define PIKO_PARSER_H

#include "ast.h"
#include "lexer.h"

typedef struct {
	Lexer lexer;
	Token current;
	Token previous;
	bool had_error;
	bool panic_mode;
} Parser;

void parser_init(Parser *p, const char *source);
Node *parse_program(Parser *p);

// Convenience helper for CLI parsing mode.
Node *parse_source(const char *source, bool *had_error);

#endif
