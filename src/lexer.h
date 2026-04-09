#ifndef PIKO_LEXER_H
#define PIKO_LEXER_H

#include "common.h"

typedef enum {
	// Single-character tokens.
	TOK_LPAREN,
	TOK_RPAREN,
	TOK_LBRACE,
	TOK_RBRACE,
	TOK_LBRACKET,
	TOK_RBRACKET,
	TOK_COMMA,
	TOK_DOT,
	TOK_SEMICOLON,
	TOK_COLON,

	// One-or-two-character tokens.
	TOK_BANG,
	TOK_BANG_EQ,
	TOK_EQ,
	TOK_EQ_EQ,
	TOK_GT,
	TOK_GT_EQ,
	TOK_LT,
	TOK_LT_EQ,
	TOK_PIPE_GT,
	TOK_PLUS,
	TOK_MINUS,
	TOK_STAR,
	TOK_SLASH,
	TOK_PERCENT,

	// Literals.
	TOK_INT,
	TOK_FLOAT,
	TOK_STRING,
	TOK_FSTRING,
	TOK_IDENT,

	// Keywords.
	TOK_AND,
	TOK_OR,
	TOK_IF,
	TOK_ELSE,
	TOK_WHILE,
	TOK_FOR,
	TOK_IN,
	TOK_FN,
	TOK_RETURN,
	TOK_LET,
	TOK_TRUE,
	TOK_FALSE,
	TOK_NIL,
	TOK_PRINT,

	TOK_EOF,
	TOK_ERROR,
} TokenType;

typedef struct {
	TokenType type;
	const char *start;
	int length;
	int line;
} Token;

typedef struct {
	const char *start;
	const char *current;
	int line;
} Lexer;

void lexer_init(Lexer *l, const char *source);
Token lexer_next(Lexer *l);
const char *token_type_name(TokenType type);
void lexer_dump(const char *source);

#endif
