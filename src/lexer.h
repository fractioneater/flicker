#ifndef flicker_lexer_h
#define flicker_lexer_h

#include "value.h"

typedef enum {
  // Single-character tokens (0 - 14)
  TOKEN_LEFT_PAREN, TOKEN_RIGHT_PAREN,
  TOKEN_LEFT_BRACKET, TOKEN_RIGHT_BRACKET,
  TOKEN_LEFT_BRACE, TOKEN_RIGHT_BRACE,
  TOKEN_SEMICOLON, TOKEN_COMMA, TOKEN_PLUS,
  TOKEN_SLASH, TOKEN_PERCENT, TOKEN_PIPE,
  TOKEN_CARET, TOKEN_AMPERSAND, TOKEN_TILDE,
  // One or two (or three) character tokens (15 - 31)
  TOKEN_DOT, TOKEN_DOT_DOT, TOKEN_DOT_DOT_LT,
  TOKEN_COLON, TOKEN_COLON_COLON,
  TOKEN_STAR, TOKEN_STAR_STAR,
  TOKEN_MINUS, TOKEN_RIGHT_ARROW,
  TOKEN_BANG, TOKEN_BANG_EQ,
  TOKEN_EQ, TOKEN_EQ_EQ,
  TOKEN_GT, TOKEN_GT_EQ,
  TOKEN_LT, TOKEN_LT_EQ,
  // Literals (32 - 35)
  TOKEN_IDENTIFIER, TOKEN_STRING, TOKEN_INTERPOLATION, TOKEN_NUMBER,
  // Keywords (36 - 66)
  TOKEN_AND, TOKEN_ATTRIBUTE, TOKEN_BREAK, TOKEN_CLASS, TOKEN_CONTINUE, TOKEN_DO,
  TOKEN_EACH, TOKEN_ELIF, TOKEN_ELSE, TOKEN_FALSE, TOKEN_FOR, TOKEN_FUN, TOKEN_IF,
  TOKEN_IN, TOKEN_IS, TOKEN_NONE, TOKEN_NOT, TOKEN_OR, TOKEN_PASS, TOKEN_PRINT,
  TOKEN_PRINT_ERROR, TOKEN_RETURN, TOKEN_SHL, TOKEN_SHR, TOKEN_STATIC, TOKEN_SUPER,
  TOKEN_THIS, TOKEN_TRUE, TOKEN_VAR, TOKEN_WHEN, TOKEN_WHILE,
  // Whitespace (67 - 69)
  TOKEN_INDENT, TOKEN_DEDENT, TOKEN_LINE,
  // (70 - 72)
  TOKEN_ERROR, TOKEN_EOF, TOKEN_NULL
} TokenType;

typedef struct {
  TokenType type;
  const char* start;
  int length;
  int line;
  Value value;
} Token;

void initLexer(const char* source);
Token nextToken();

#endif
