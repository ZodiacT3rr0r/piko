#include "lexer.h"

typedef struct {
	const char *word;
	int len;
	TokenType type;
} Keyword;

static const Keyword keywords[] = {
	{"and", 3, TOK_AND},
	{"or", 2, TOK_OR},
	{"if", 2, TOK_IF},
	{"else", 4, TOK_ELSE},
	{"while", 5, TOK_WHILE},
	{"for", 3, TOK_FOR},
	{"in", 2, TOK_IN},
	{"fn", 2, TOK_FN},
	{"return", 6, TOK_RETURN},
	{"let", 3, TOK_LET},
	{"true", 4, TOK_TRUE},
	{"false", 5, TOK_FALSE},
	{"nil", 3, TOK_NIL},
	{"print", 5, TOK_PRINT},
	{NULL, 0, TOK_ERROR},
};

static bool is_at_end(Lexer *l) {
	return *l->current == '\0';
}

static char advance_char(Lexer *l) {
	l->current++;
	return l->current[-1];
}

static char peek_char(Lexer *l) {
	return *l->current;
}

static char peek_next_char(Lexer *l) {
	if (is_at_end(l)) {
		return '\0';
	}
	return l->current[1];
}

static bool match_char(Lexer *l, char expected) {
	if (is_at_end(l)) {
		return false;
	}
	if (*l->current != expected) {
		return false;
	}
	l->current++;
	return true;
}

static Token make_token(TokenType type, const char *start, int length, int line) {
	Token t;
	t.type = type;
	t.start = start;
	t.length = length;
	t.line = line;
	return t;
}

static Token error_token(const char *message, int line) {
	return make_token(TOK_ERROR, message, (int)strlen(message), line);
}

static bool is_digit(char c) {
	return c >= '0' && c <= '9';
}

static bool is_alpha(char c) {
	return (c >= 'a' && c <= 'z') ||
		   (c >= 'A' && c <= 'Z') ||
		   c == '_';
}

static void skip_whitespace(Lexer *l) {
	for (;;) {
		char c = peek_char(l);
		switch (c) {
			case ' ':
			case '\r':
			case '\t':
				advance_char(l);
				break;
			case '\n':
				l->line++;
				advance_char(l);
				break;
			case '/':
				if (peek_next_char(l) == '/') {
					while (peek_char(l) != '\n' && !is_at_end(l)) {
						advance_char(l);
					}
				} else {
					return;
				}
				break;
			default:
				return;
		}
	}
}

static TokenType identifier_type(const char *start, int length) {
	for (int i = 0; keywords[i].word != NULL; i++) {
		if (length == keywords[i].len &&
			memcmp(start, keywords[i].word, (size_t)length) == 0) {
			return keywords[i].type;
		}
	}
	return TOK_IDENT;
}

static Token identifier(Lexer *l, const char *start, int line) {
	while (is_alpha(peek_char(l)) || is_digit(peek_char(l))) {
		advance_char(l);
	}
	int length = (int)(l->current - start);
	return make_token(identifier_type(start, length), start, length, line);
}

static Token number(Lexer *l, const char *start, int line) {
	while (is_digit(peek_char(l))) {
		advance_char(l);
	}

	if (peek_char(l) == '.' && is_digit(peek_next_char(l))) {
		advance_char(l);
		while (is_digit(peek_char(l))) {
			advance_char(l);
		}
		return make_token(TOK_FLOAT, start, (int)(l->current - start), line);
	}

	return make_token(TOK_INT, start, (int)(l->current - start), line);
}

static Token string_like(Lexer *l, int line, TokenType type) {
	const char *content_start = l->current;
	while (!is_at_end(l) && peek_char(l) != '"') {
		char c = advance_char(l);
		if (c == '\\') {
			if (is_at_end(l)) {
				break;
			}
			char escaped = advance_char(l);
			if (escaped == '\n') {
				l->line++;
			}
			continue;
		}
		if (c == '\n') {
			l->line++;
		}
	}

	if (is_at_end(l)) {
		return error_token("Unterminated string.", line);
	}

	int length = (int)(l->current - content_start);
	advance_char(l);
	return make_token(type, content_start, length, line);
}

void lexer_init(Lexer *l, const char *source) {
	l->start = source;
	l->current = source;
	l->line = 1;
}

Token lexer_next(Lexer *l) {
	skip_whitespace(l);

	l->start = l->current;
	int token_line = l->line;

	if (is_at_end(l)) {
		return make_token(TOK_EOF, l->current, 0, token_line);
	}

	char c = advance_char(l);

	if (c == 'f' && peek_char(l) == '"') {
		advance_char(l);
		return string_like(l, token_line, TOK_FSTRING);
	}

	if (is_alpha(c)) {
		return identifier(l, l->start, token_line);
	}

	if (is_digit(c)) {
		return number(l, l->start, token_line);
	}

	switch (c) {
		case '(':
			return make_token(TOK_LPAREN, l->start, 1, token_line);
		case ')':
			return make_token(TOK_RPAREN, l->start, 1, token_line);
		case '{':
			return make_token(TOK_LBRACE, l->start, 1, token_line);
		case '}':
			return make_token(TOK_RBRACE, l->start, 1, token_line);
		case '[':
			return make_token(TOK_LBRACKET, l->start, 1, token_line);
		case ']':
			return make_token(TOK_RBRACKET, l->start, 1, token_line);
		case ',':
			return make_token(TOK_COMMA, l->start, 1, token_line);
		case '.':
			return make_token(TOK_DOT, l->start, 1, token_line);
		case ';':
			return make_token(TOK_SEMICOLON, l->start, 1, token_line);
		case ':':
			return make_token(TOK_COLON, l->start, 1, token_line);
		case '!':
			if (match_char(l, '=')) {
				return make_token(TOK_BANG_EQ, l->start, 2, token_line);
			}
			return make_token(TOK_BANG, l->start, 1, token_line);
		case '=':
			if (match_char(l, '=')) {
				return make_token(TOK_EQ_EQ, l->start, 2, token_line);
			}
			return make_token(TOK_EQ, l->start, 1, token_line);
		case '>':
			if (match_char(l, '=')) {
				return make_token(TOK_GT_EQ, l->start, 2, token_line);
			}
			return make_token(TOK_GT, l->start, 1, token_line);
		case '<':
			if (match_char(l, '=')) {
				return make_token(TOK_LT_EQ, l->start, 2, token_line);
			}
			return make_token(TOK_LT, l->start, 1, token_line);
		case '|':
			if (match_char(l, '>')) {
				return make_token(TOK_PIPE_GT, l->start, 2, token_line);
			}
			return error_token("Unexpected '|'. Did you mean '|>'?", token_line);
		case '+':
			return make_token(TOK_PLUS, l->start, 1, token_line);
		case '-':
			return make_token(TOK_MINUS, l->start, 1, token_line);
		case '*':
			return make_token(TOK_STAR, l->start, 1, token_line);
		case '/':
			return make_token(TOK_SLASH, l->start, 1, token_line);
		case '%':
			return make_token(TOK_PERCENT, l->start, 1, token_line);
		case '"':
			return string_like(l, token_line, TOK_STRING);
		default:
			return error_token("Unexpected character.", token_line);
	}
}

const char *token_type_name(TokenType type) {
	switch (type) {
		case TOK_LPAREN: return "TOK_LPAREN";
		case TOK_RPAREN: return "TOK_RPAREN";
		case TOK_LBRACE: return "TOK_LBRACE";
		case TOK_RBRACE: return "TOK_RBRACE";
		case TOK_LBRACKET: return "TOK_LBRACKET";
		case TOK_RBRACKET: return "TOK_RBRACKET";
		case TOK_COMMA: return "TOK_COMMA";
		case TOK_DOT: return "TOK_DOT";
		case TOK_SEMICOLON: return "TOK_SEMICOLON";
		case TOK_COLON: return "TOK_COLON";
		case TOK_BANG: return "TOK_BANG";
		case TOK_BANG_EQ: return "TOK_BANG_EQ";
		case TOK_EQ: return "TOK_EQ";
		case TOK_EQ_EQ: return "TOK_EQ_EQ";
		case TOK_GT: return "TOK_GT";
		case TOK_GT_EQ: return "TOK_GT_EQ";
		case TOK_LT: return "TOK_LT";
		case TOK_LT_EQ: return "TOK_LT_EQ";
		case TOK_PIPE_GT: return "TOK_PIPE_GT";
		case TOK_PLUS: return "TOK_PLUS";
		case TOK_MINUS: return "TOK_MINUS";
		case TOK_STAR: return "TOK_STAR";
		case TOK_SLASH: return "TOK_SLASH";
		case TOK_PERCENT: return "TOK_PERCENT";
		case TOK_INT: return "TOK_INT";
		case TOK_FLOAT: return "TOK_FLOAT";
		case TOK_STRING: return "TOK_STRING";
		case TOK_FSTRING: return "TOK_FSTRING";
		case TOK_IDENT: return "TOK_IDENT";
		case TOK_AND: return "TOK_AND";
		case TOK_OR: return "TOK_OR";
		case TOK_IF: return "TOK_IF";
		case TOK_ELSE: return "TOK_ELSE";
		case TOK_WHILE: return "TOK_WHILE";
		case TOK_FOR: return "TOK_FOR";
		case TOK_IN: return "TOK_IN";
		case TOK_FN: return "TOK_FN";
		case TOK_RETURN: return "TOK_RETURN";
		case TOK_LET: return "TOK_LET";
		case TOK_TRUE: return "TOK_TRUE";
		case TOK_FALSE: return "TOK_FALSE";
		case TOK_NIL: return "TOK_NIL";
		case TOK_PRINT: return "TOK_PRINT";
		case TOK_EOF: return "TOK_EOF";
		case TOK_ERROR: return "TOK_ERROR";
	}
	return "TOK_UNKNOWN";
}

void lexer_dump(const char *source) {
	Lexer l;
	lexer_init(&l, source);

	for (;;) {
		Token t = lexer_next(&l);
		const char *name = token_type_name(t.type);

		if (t.type == TOK_EOF) {
			printf("%4d  %-12s  <eof>\n", t.line, name);
			break;
		}

		if (t.type == TOK_ERROR) {
			printf("%4d  %-12s  %.*s\n", t.line, name, t.length, t.start);
			break;
		}

		printf("%4d  %-12s  %.*s\n", t.line, name, t.length, t.start);
	}
}
