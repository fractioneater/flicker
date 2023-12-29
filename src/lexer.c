#include "lexer.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "memory.h"
#include "object.h"
#include "utils.h"

#define MAX_INTERPOLATION_NESTING 8

typedef struct {
  const char* start;
  const char* current;
  int line;

  // The lexer needs some variables to keep track of interpolation.
  int parens[MAX_INTERPOLATION_NESTING];
  int parenCount;

  // The lexer also needs to keep track of the indentation.
  bool checkIndent;

  // How many dedents are waiting to be scanned.
  int dedentCount;

  // All of the past indentation levels (to determine how many dedents are created).
  IntArray indents;
} Lexer;

Lexer lexer;

void initLexer(const char* source) {
  lexer.start = source;
  lexer.current = source;
  lexer.line = 1;
  lexer.parenCount = 0;
  lexer.checkIndent = true;
  lexer.dedentCount = 0;
  intArrayInit(&lexer.indents);
  intArrayWrite(&lexer.indents, 0);
}

void freeLexer() {
  intArrayFree(&lexer.indents);
}

static bool isAlpha(char c) {
  return ('a' <= c && c <= 'z') ||
         ('A' <= c && c <= 'Z') ||
         c == '_';
}

static bool isDigit(char c) { return c == '_' || ('0' <= c && c <= '9'); }

static bool atEnd() { return *lexer.current == '\0'; }

static char advance() {
  lexer.current++;
  if (lexer.current[-1] == '\n') lexer.line++;
  return lexer.current[-1];
}

static char peek() { return *lexer.current; }

static char peekNext() {
  if (atEnd()) return '\0';
  return lexer.current[1];
}

static bool match(char expected) {
  if (atEnd()) return false;
  if (*lexer.current != expected) return false;
  lexer.current++;
  return true;
}

static Token makeToken(TokenType type) {
  Token token;
  token.type = type;
  token.start = lexer.start;
  token.length = (int)(lexer.current - lexer.start);

  if (type == TOKEN_LINE) token.line = lexer.line - 1;
  else token.line = lexer.line;

  return token;
}

static Token errorToken(const char* message) {
  Token token;
  token.type = TOKEN_ERROR;
  token.start = message;
  token.length = (int)strlen(message);
  token.line = lexer.line;
  return token;
}

static Token nullToken() {
  Token token;
  token.type = TOKEN_NULL;
  return token;
}

static inline bool notNullToken(Token token) {
  return token.type != TOKEN_NULL;
}

#define MAX_COMMENT_NEST 16

static Token blockComment() {
  int nestDepth = 1;
  // Keep consuming until the closing comment or end of file.
  while (nestDepth > 0) {
    if (atEnd()) {
      return errorToken("Unclosed block comment");
    }

    if (peek() == '#') {
      if (peekNext() == ':') {
        advance();
        advance();
        if (nestDepth++ == MAX_COMMENT_NEST) {
          return errorToken("Too many nested comments");
        }
        continue;
      } else {
        advance();
        nestDepth--;
        continue;
      }
    }

    advance();
  }

  return nullToken();
}

static TokenType checkKeyword(int start, int length, const char* rest, TokenType type) {
  if (lexer.current - lexer.start == start + length && memcmp(lexer.start + start, rest, length) == 0) {
    return type;
  }

  return TOKEN_IDENTIFIER;
}

static TokenType identifierType() {
  switch (lexer.start[0]) {
    case 'a':
      if (lexer.current - lexer.start > 2) {
        switch (lexer.start[1]) {
          case 'n': return checkKeyword(2, 1, "d", TOKEN_AND);
          case 't': return checkKeyword(2, 7, "tribute", TOKEN_ATTRIBUTE);
        }
      }
      break;
    case 'b': return checkKeyword(1, 4, "reak", TOKEN_BREAK);
    case 'c':
      if (lexer.current - lexer.start > 4) {
        switch (lexer.start[1]) {
          case 'l': return checkKeyword(2, 3, "ass", TOKEN_CLASS);
          case 'o': return checkKeyword(2, 6, "ntinue", TOKEN_CONTINUE);
        }
      }
      break;
    case 'd': return checkKeyword(1, 1, "o", TOKEN_DO);
    case 'e':
      if (lexer.start[1] == 'a') {
        if (lexer.current - lexer.start > 2) return checkKeyword(2, 2, "ch", TOKEN_EACH);
      } else if (lexer.start[1] == 'l') {
        if (lexer.current - lexer.start > 2) {
          switch (lexer.start[2]) {
            case 'i': return checkKeyword(3, 1, "f", TOKEN_ELIF);
            case 's': return checkKeyword(3, 1, "e", TOKEN_ELSE);
          }
        }
      } else if (lexer.start[1] == 'r') {
        if (lexer.current - lexer.start > 4) return checkKeyword(2, 3, "ror", TOKEN_PRINT_ERROR);
      }
      break;
    case 'F': return checkKeyword(1, 4, "alse", TOKEN_FALSE);
    case 'f':
      if (lexer.current - lexer.start > 1) {
        switch (lexer.start[1]) {
          case 'o': return checkKeyword(2, 1, "r", TOKEN_FOR);
          case 'u': return checkKeyword(2, 1, "n", TOKEN_FUN);
        }
      }
      break;
    case 'i':
      if (lexer.current - lexer.start > 0) {
        switch (lexer.start[1]) {
          case 'f': return checkKeyword(2, 0, "", TOKEN_IF);
          case 'n': return checkKeyword(2, 0, "", TOKEN_IN);
          case 's': return checkKeyword(2, 0, "", TOKEN_IS);
        }
      }
      break;
    case 'N': return checkKeyword(1, 3, "one", TOKEN_NONE);
    case 'n': return checkKeyword(1, 2, "ot", TOKEN_NOT);
    case 'o': return checkKeyword(1, 1, "r", TOKEN_OR);
    case 'p':
      if (lexer.current - lexer.start > 3) {
        switch (lexer.start[1]) {
          case 'a': return checkKeyword(2, 2, "ss", TOKEN_PASS);
          case 'r': return checkKeyword(2, 3, "int", TOKEN_PRINT);
        }
      }
      break;
    case 'r': return checkKeyword(1, 5, "eturn", TOKEN_RETURN);
    case 's':
      if (lexer.current - lexer.start > 2) {
        switch (lexer.start[1]) {
          case 't': return checkKeyword(2, 4, "atic", TOKEN_STATIC);
          case 'u': return checkKeyword(2, 3, "per", TOKEN_SUPER);
          case 'h':
            if (lexer.current - lexer.start > 1) {
              switch (lexer.start[2]) {
                case 'l': return checkKeyword(3, 0, "", TOKEN_SHL);
                case 'r': return checkKeyword(3, 0, "", TOKEN_SHR);
              }
            }
            break;
        }
      }
      break;
    case 'T': return checkKeyword(1, 3, "rue", TOKEN_TRUE);
    case 't': return checkKeyword(1, 3, "his", TOKEN_THIS);
    case 'u': return checkKeyword(1, 2, "se", TOKEN_USE);
    case 'v':
      if (lexer.current - lexer.start == 3 && lexer.start[1] == 'a') {
        switch (lexer.start[2]) {
          case 'l': return TOKEN_VAL;
          case 'r': return TOKEN_VAR;
          default: return TOKEN_IDENTIFIER;
        }
      }
      break;
    return checkKeyword(1, 2, "ar", TOKEN_VAR);
    case 'w':
      if (lexer.start[1] == 'h') {
        if (lexer.current - lexer.start > 3) {
          switch (lexer.start[2]) {
            case 'e': return checkKeyword(3, 1, "n", TOKEN_WHEN);
            case 'i': return checkKeyword(3, 2, "le", TOKEN_WHILE);
          }
        }
      }
      break;
  }

  return TOKEN_IDENTIFIER;
}

static Token identifier() {
  while (isAlpha(peek()) || isDigit(peek())) advance();
  return makeToken(identifierType());
}

static Token forceIdentifier() {
  Token token = nullToken();
  while (peek() != '`' && !atEnd()) {
    if (token.type == TOKEN_NULL) {
      // There hasn't been an error yet.
      if (peek() == '\n') {
        token = errorToken("Can't have linebreaks in identifiers");
      } else if (peek() == '(' || peek() == ')') {
        token = errorToken("Can't have parentheses in identifiers");
      }
    }
    advance();
  }

  if (atEnd()) return errorToken("Unterminated identifier");

  // The closing backtick.
  advance();

  if (token.type != TOKEN_NULL) return token;
  token = makeToken(TOKEN_IDENTIFIER);
  token.start += 1;
  token.length -= 2;
  return token;
}

static int hexDigit() {
  char c = advance();
  if ('0' <= c && c <= '9') return c - '0';
  if ('a' <= c && c <= 'z') return c - 'a' + 10;
  if ('A' <= c && c <= 'Z') return c - 'A' + 10;

  lexer.current--;
  return -1;
}

static Token makeNumber(bool isHex) {
  errno = 0;
  Token token = makeToken(TOKEN_NUMBER);

  if (isHex) {
    token.value = NUMBER_VAL((double)strtoll(lexer.start, NULL, 16));
  } else {
    size_t len = lexer.current - lexer.start + 1;
    char* copy = ALLOCATE(char, len);
    memcpy(copy, lexer.start, len);
    copy[len - 1] = '\0';

    char *read = copy, *write = copy;
    while (*read) {
      *write = *read++;
      write += (*write != '_');
    }

    token.value = NUMBER_VAL(strtod(copy, NULL));
    FREE_ARRAY(char, copy, len);
  }

  if (errno == ERANGE) {
    return errorToken("Number literal is too large");
  }

  return token;
}

static Token hexNumber() {
  advance();

  while (hexDigit() != -1);

  return makeNumber(true);
}

static Token number() {
  while (isDigit(peek())) advance();

  // Look for a fractional part.
  if (peek() == '.' && isDigit(peekNext())) {
    // Consume the ".".
    advance();

    while (isDigit(peek())) advance();
  }

  return makeNumber(false);
}

static int hexEscape(int digits) {
  int value = 0;
  for (int i = 0; i < digits; i++) {
    if (peek() == '"' || atEnd()) {
      errorToken("Incomplete escape sequence"); // This doesn't work.
      break;
    }

    int digit = hexDigit();
    if (digit == -1) {
      errorToken("Invalid escape sequence");
      break;
    }

    value = (value * 16) | digit;
  }

  return value;
}

static void unicodeEscape(ByteArray* string, int length) {
  int value = hexEscape(length);

  int numBytes = utf8EncodeNumBytes(value);
  if (numBytes != 0) {
    byteArrayFill(string, 0, numBytes);
    utf8Encode(value, string->data + string->count - numBytes);
  }
}

static Token string() {
  ByteArray string;
  TokenType type = TOKEN_STRING;
  byteArrayInit(&string);

  for (;;) {
    char c = advance();
    if (c == '"') break;
    if (c == '\r') continue;

    if (atEnd()) return errorToken("Unterminated string");

    if (c == '=' && peek() == '(') {
      if (lexer.parenCount < MAX_INTERPOLATION_NESTING) {
        type = TOKEN_INTERPOLATION;
        advance();
        lexer.parens[lexer.parenCount++] = 1;
        break;
      }

      return errorToken("Too many nested strings");
    }

    if (c == '\\') {
      switch (advance()) {
        case '\\': byteArrayWrite(&string, '\\'); break;
        case '"':  byteArrayWrite(&string, '"'); break;
        case '=':  byteArrayWrite(&string, '='); break;
        case '0':  byteArrayWrite(&string, '\0'); break;
        case 'a':  byteArrayWrite(&string, '\a'); break;
        case 'b':  byteArrayWrite(&string, '\b'); break;
        case 'e':  byteArrayWrite(&string, '\033'); break;
        case 'f':  byteArrayWrite(&string, '\f'); break;
        case 'n':  byteArrayWrite(&string, '\n'); break;
        case 'r':  byteArrayWrite(&string, '\r'); break;
        case 't':  byteArrayWrite(&string, '\t'); break;
        case 'u':  unicodeEscape(&string, 4); break;
        case 'U':  unicodeEscape(&string, 8); break;
        case 'v':  byteArrayWrite(&string, '\v'); break;
        case 'x':  byteArrayWrite(&string, (uint8_t)hexEscape(2));
        default:
          return errorToken("Invalid escape character");
      }
    } else {
      byteArrayWrite(&string, c);
    }
  }

  Value value = OBJ_VAL(copyStringLength((char*)string.data, string.count));
  byteArrayClear(&string);
  Token token = makeToken(type);
  token.value = value;
  return token;
}

Token indentation() {
  int indent = 0;
  char c = peek();

  while (c == ' ' || c == '\t' || c == '\r') {
    advance();
    if (c == ' ') indent += 1;
    else if (c == '\t') indent += 4;

    c = peek();
  }

  if (c == '\n' || (c == '#' && peekNext() != ':')) {
    // This line doesn't count, it's just indentation or a full-line comment.
    if (c != '\n') {
      while (peek() != '\n' && !atEnd()) advance();
    }

    // Move on to the next line and try again.
    advance();
    return indentation();
  }

  if (indent > lexer.indents.data[lexer.indents.count - 1]) {
    // You can only have 1 indent token at a time, so stop looking for indentation.
    lexer.checkIndent = false;

    intArrayWrite(&lexer.indents, indent);
    return makeToken(TOKEN_INDENT);
  } else if (indent < lexer.indents.data[lexer.indents.count - 1]) {
    int i = lexer.indents.count - 1;
    int innerIndent = lexer.indents.data[i];

    while (indent != lexer.indents.data[i]) {
      i--;
      if (i == -1) {
        lexer.dedentCount = 0;
        return errorToken("Invalid indentation");
      }

      if (lexer.indents.data[i] < innerIndent) {
        innerIndent = lexer.indents.data[i];

        intArrayWrite(&lexer.indents, indent);
        lexer.dedentCount++;
      }
    }

    if (lexer.dedentCount) {
      // We've already checked indents; the only thing left to do is make tokens.
      lexer.checkIndent = false;

      lexer.dedentCount--;
      return makeToken(TOKEN_DEDENT);
    }
  }

  // No indent, stop looking.
  lexer.checkIndent = false;
  return nullToken();
}

Token nextToken() {
  if (lexer.dedentCount) {
    lexer.dedentCount--;
    return makeToken(TOKEN_DEDENT);
  }

  if (lexer.checkIndent) {
    Token indent = indentation();
    if (notNullToken(indent)) return indent;
    else lexer.checkIndent = false;
  }

  if (atEnd()) return makeToken(TOKEN_EOF);

  while (!atEnd()) {
    lexer.start = lexer.current;

    char c = advance();

    switch (c) {
      case '(': {
        if (lexer.parenCount > 0) lexer.parens[lexer.parenCount - 1]++;
        return makeToken(TOKEN_LEFT_PAREN);
      }
      case ')': {
        if (lexer.parenCount > 0 && --lexer.parens[lexer.parenCount - 1] == 0) {
          lexer.parenCount--;
          return string();
        }
        return makeToken(TOKEN_RIGHT_PAREN);
      }
      case '[': return makeToken(TOKEN_LEFT_BRACKET);
      case ']': return makeToken(TOKEN_RIGHT_BRACKET);
      case '{': return makeToken(TOKEN_LEFT_BRACE);
      case '}': return makeToken(TOKEN_RIGHT_BRACE);
      case ';': return makeToken(TOKEN_SEMICOLON);
      case ',': return makeToken(TOKEN_COMMA);
      case '+': return makeToken(TOKEN_PLUS);
      case '/': return makeToken(TOKEN_SLASH);
      case '%': return makeToken(TOKEN_PERCENT);
      case '|': return makeToken(TOKEN_PIPE);
      case '^': return makeToken(TOKEN_CARET);
      case '&': return makeToken(TOKEN_AMPERSAND);
      case '~': return makeToken(TOKEN_TILDE);
      case '.': return makeToken(match('.') ? (match('<') ? TOKEN_DOT_DOT_LT : TOKEN_DOT_DOT) : TOKEN_DOT);
      case ':': return makeToken(match(':') ? TOKEN_COLON_COLON : TOKEN_COLON);
      case '*': return makeToken(match('*') ? TOKEN_STAR_STAR : TOKEN_STAR);
      case '-': return makeToken(match('>') ? TOKEN_RIGHT_ARROW : TOKEN_MINUS);
      case '!': return makeToken(match('=') ? TOKEN_BANG_EQ : TOKEN_BANG);
      case '=': return makeToken(match('=') ? TOKEN_EQ_EQ : TOKEN_EQ);
      case '<': return makeToken(match('=') ? TOKEN_LT_EQ : TOKEN_LT);
      case '>': return makeToken(match('=') ? TOKEN_GT_EQ : TOKEN_GT);
      case '"': return string();
      case '`': return forceIdentifier();
      case '#':
        advance();
        if (peek() == ':') {
          Token error = blockComment();
          if (notNullToken(error)) return error;
        } else {
          // Consume until the end of the line.
          while (peek() != '\n' && !atEnd()) advance();
        }
        break;
      case '\n':
        lexer.checkIndent = true;
        return makeToken(TOKEN_LINE);
      case ' ':
      case '\t':
      case '\r':
        while (peek() == ' ' || peek() == '\t' || peek() == '\r') {
          advance();
        }
        break;
      case '0':
        if (peek() == 'x') return hexNumber();
        return number();
      default:
        if (isAlpha(c)) return identifier();
        if (isDigit(c)) return number();
        return errorToken("Unexpected character");
    }
  }

  lexer.start = lexer.current;
  return makeToken(TOKEN_EOF);
}
