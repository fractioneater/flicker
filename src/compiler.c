#include "compiler.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "lexer.h"
#include "memory.h"
#include "utils.h"

#if DEBUG_PRINT_CODE
#  include "debug.h"
#endif

typedef struct {
  // The most recently lexed token
  Token current;

  // The most recently consumed token.
  Token previous;

  // The module being parsed.
  ObjModule* module;

  // How many of the following dedent tokens will be ignored.
  int ignoreDedents;

  // Print the value returned by the code.
  bool printResult;

  // If an expression was parsed most recently.
  bool onExpression;

  // If a compiler error has appeared.
  bool hadError;
  bool panicMode;
} Parser;

typedef enum {
  BP_NONE,
  BP_ASSIGNMENT,  // =
  BP_IF,          // if ... else
  BP_NOT,         // not
  BP_OR,          // or
  BP_AND,         // and
  BP_EQUALITY,    // == !=
  BP_COMPARISON,  // < > <= >=
  BP_IS,          // is
  BP_IN,          // in
  BP_BIT_OR,      // |
  BP_BIT_XOR,     // ^
  BP_BIT_AND,     // &
  BP_BIT_SHIFT,   // shl shr
  BP_RANGE,       // .. :
  BP_TERM,        // + -
  BP_FACTOR,      // * / %
  BP_EXPONENT,    // **
  BP_UNARY,       // -
  BP_CALL,        // . ()
  BP_PRIMARY
} BindingPower;

typedef enum {
  SIG_METHOD,
  SIG_ATTRIBUTE
} SignatureType;

typedef struct {
  const char* name;
  int length;
  SignatureType type;
  int arity;
  bool* asProperty;
} Signature;

typedef void (*ParseFn)(bool canAssign);

typedef void (*SignatureFn)(Signature* signature);

typedef struct {
  ParseFn prefix;
  ParseFn infix;
  BindingPower bp;
  const char* name;
  SignatureFn signatureFn;
} ParseRule;

typedef struct {
  // The name of the local variable.
  Token name;

  // The scope depth that this variable was declared in.
  int depth;

  // Whether or not this local can be reassigned.
  bool isMutable;

  // If this local is being used as an upvalue.
  bool isCaptured;
} Local;

typedef struct {
  // The index of the local or upvalue, specific to the current function.
  uint8_t index;

  // If the upvalue is capturing a local variable from the enclosing function.
  bool isLocal;
} Upvalue;

typedef struct Loop {
  // The instruction that the loop should jump back to.
  int start;

  // The index of the jump instruction used to exit the loop.
  int exitJump;

  // Depth of the scope that is exited by a break statement.
  int scopeDepth;

  // The count and capacity of the break stack.
  int breakCount;
  int breakCapacity;

  // The indices of breaks in the loop body, to be patched at the end.
  int* breaks;

  // The label for the loop, used with break and continue statements.
  Token* label;

  // The loop enclosing this one. NULL if this is the outermost.
  struct Loop* enclosing;
} Loop;

typedef enum {
  TYPE_FUNCTION,
  TYPE_LAMBDA,
  TYPE_INITIALIZER,
  TYPE_METHOD,
  TYPE_STATIC_METHOD,
  TYPE_SCRIPT
} FunctionType;

typedef struct Compiler {
  // The compiler for the enclosing function.
  struct Compiler* enclosing;

  // The innermost loop being compiled.
  Loop* loop;

  // The function being compiled.
  ObjFunction* function;
  FunctionType type;

  // The local variables in scope.
  Local locals[UINT8_COUNT];

  // The number of locals in scope currently.
  int localCount;

  // The upvalues that have been captured from outside scopes.
  Upvalue upvalues[UINT8_COUNT];

  // The array the compiler is writing bytecode to, NULL if the
  // compiler is writing to the main chunk.
  ByteArray* customWrite;
  IntArray* customLine;

  // The level of block scope nesting.
  int scopeDepth;
} Compiler;

typedef struct ClassCompiler {
  bool hasInitializer;
  struct ClassCompiler* enclosing;
} ClassCompiler;

Parser parser;
Compiler* current = NULL;
ClassCompiler* currentClass = NULL;

static inline Chunk* currentChunk() {
  return &current->function->chunk;
}

static void errorAt(Token* token, const char* format, va_list args) {
  if (parser.panicMode) return;
  parser.panicMode = true;
  fprintf(stderr, "\033[1m%s:%d:\033[0m ", parser.module->name->chars, token->line);

  if (token->type == TOKEN_EOF) {
    fprintf(stderr, "error at end");
  } else if (token->type == TOKEN_LINE) {
    fprintf(stderr, "error at newline");
  } else if (token->type == TOKEN_INDENT || token->type == TOKEN_DEDENT) {
    fprintf(stderr, "error at indentation");
  } else if (token->type == TOKEN_ERROR) {
    fprintf(stderr, "error");
  } else {
    fprintf(stderr, "error at '%.*s'", token->length, token->start);
  }

  fprintf(stderr, ": ");
  vfprintf(stderr, format, args);
  fprintf(stderr, "\n");
  parser.hadError = true;
}

static void error(const char* format, ...) {
  va_list args;
  va_start(args, format);
  errorAt(&parser.previous, format, args);
  va_end(args);
}

static void errorAtCurrent(const char* format, ...) {
  va_list args;
  va_start(args, format);
  errorAt(&parser.current, format, args);
  va_end(args);
}

static void advance() {
  parser.previous = parser.current;

  for (;;) {
    parser.current = nextToken();
    if (parser.ignoreDedents > 0 && parser.current.type == TOKEN_DEDENT) {
      parser.ignoreDedents--;
      continue;
    }

    if (parser.current.type != TOKEN_ERROR) break;

    errorAtCurrent(parser.current.start);
  }
}

static void expect(TokenType type, const char* message) {
  if (parser.current.type == type) {
    advance();
    return;
  }

  errorAtCurrent(message);
}

static bool check(TokenType type) {
  return parser.current.type == type;
}

static bool match(TokenType type) {
  if (!check(type)) return false;
  advance();
  return true;
}

static bool matchLine() {
  if (!match(TOKEN_LINE)) return false;

  while (match(TOKEN_LINE));
  return true;
}

static void expectLine(const char* message) {
  if (!matchLine()) {
    errorAtCurrent(message);
  }
}

static bool ignoreIndentation() {
  if (!match(TOKEN_INDENT)) {
    if (!match(TOKEN_DEDENT)) return false;
    while (match(TOKEN_DEDENT));
  }
  return true;
}

static void expectStatementEnd(const char* message) {
  // If the parser has just synchronized after an error, it might have
  // already consumed a newline token. That's why we check for it here.
  if (parser.previous.type == TOKEN_LINE ||
      parser.previous.type == TOKEN_DEDENT ||
      parser.previous.type == TOKEN_INDENT)
    return;
  if (match(TOKEN_SEMICOLON)) {
    matchLine();
    return;
  }
  if (!matchLine()) errorAtCurrent(message);
}

static void emitByte(uint8_t byte) {
  if (current->customWrite != NULL) {
    byteArrayWrite(current->customWrite, byte);
    intArrayWrite(current->customLine, parser.previous.line);
    return;
  }
  writeChunk(currentChunk(), byte, parser.previous.line);
}

static void emitVariableBytes(int arg) {
  if (arg < 0x80) {
    emitByte((uint8_t)arg);
  } else {
    emitByte(((arg >> 8) & 0xff) | 0x80);
    emitByte(arg & 0xff);
  }
}

static void emitVariableArg(uint8_t instruction, int arg) {
  emitByte(instruction);
  emitVariableBytes(arg);
}

static void emitBytes(uint8_t byte1, uint8_t byte2) {
  emitByte(byte1);
  emitByte(byte2);
}

static void emitLoop(int loopStart) {
  emitByte(OP_LOOP);

  int offset = currentChunk()->count - loopStart + 2;
  if (offset > UINT16_MAX) error("Loop body is too large");

  emitByte((offset >> 8) & 0xff);
  emitByte(offset & 0xff);
}

static int emitJump(uint8_t instruction) {
  emitByte(instruction);
  emitByte(0xff);
  emitByte(0xff);
  return currentChunk()->count - 2;
}

static void emitReturn() {
  if (current->type == TYPE_INITIALIZER) {
    emitBytes(OP_GET_LOCAL, 0);
  } else {
    emitByte(OP_NONE);
  }

  emitByte(OP_RETURN);
}

static int makeConstant(Value value) {
  int constant = addConstant(currentChunk(), value);
  if (constant > MAX_CONSTANTS) {
    error("A function can only contain %d constants", MAX_CONSTANTS);
    return 0;
  }

  return constant;
}

static void emitConstant(Value value) {
  emitByte(OP_CONSTANT);
  emitVariableBytes(makeConstant(value));
}

static void emitConstantArg(uint8_t instruction, Value arg) {
  emitByte(instruction);
  emitVariableBytes(makeConstant(arg));
}

static inline void callMethod(int argCount, const char* name, int length) {
  emitConstantArg(OP_INVOKE_0 + argCount, OBJ_VAL(copyStringLength(name, length)));
}

static void patchJump(int offset) {
  // -2 to account for the bytecode for the jump offset itself.
  int jump = currentChunk()->count - offset - 2;

  if (jump > UINT16_MAX) {
    error("Too much code to jump over");
  }

  currentChunk()->code[offset] = (jump >> 8) & 0xff;
  currentChunk()->code[offset + 1] = jump & 0xff;
}

static void startLoop(Loop* loop) {
  loop->enclosing = current->loop;
  loop->start = currentChunk()->count;
  loop->scopeDepth = current->scopeDepth;
  loop->breakCount = 0;
  loop->breakCapacity = 0;
  loop->breaks = NULL;
  current->loop = loop;
}

static void endLoop() {
  emitLoop(current->loop->start);

  // Just in case you want to know the label of the loop:
  //
  // if (current->loop->label != NULL) {
  //   Token* label = current->loop->label;
  //   char* chars = ALLOCATE(char, label->length + 1);
  //   memcpy(chars, label->start, label->length);
  //   chars[label->length] = '\0';
  // }

  if (current->loop->exitJump != -1) {
    patchJump(current->loop->exitJump);
    emitByte(OP_POP); // Pop the condition.
  }

  while (current->loop->breakCount > 0) {
    int breakJump = current->loop->breaks[--current->loop->breakCount];
    patchJump(breakJump);
  }

  current->loop = current->loop->enclosing;
}

static void initCompiler(Compiler* compiler, FunctionType type) {
  compiler->enclosing = current;
  compiler->loop = NULL;
  compiler->function = NULL;
  compiler->type = type;
  compiler->localCount = 0;
  compiler->scopeDepth = 0;

  compiler->customWrite = NULL;

  compiler->function = newFunction(parser.module);
  current = compiler;

  if (type == TYPE_LAMBDA) {
    current->function->name = copyStringLength("\b", 1);
  } else if (type != TYPE_SCRIPT) {
    current->function->name = copyStringLength(parser.previous.start, parser.previous.length);
  }

  Local* local = &current->locals[current->localCount++];
  local->depth = 0;
  local->isMutable = false;
  local->isCaptured = false;
  if (type != TYPE_FUNCTION) {
    local->name.start = "this";
    local->name.length = 4;
  } else {
    local->name.start = "";
    local->name.length = 0;
  }
}

static ObjFunction* endCompiler() {
  if (current->scopeDepth == 0 && parser.onExpression && parser.printResult) {
    emitByte(OP_RETURN_OUTPUT);
    emitByte(OP_RETURN);
  } else {
    emitReturn();
  }

  ObjFunction* function = current->function;

# if DEBUG_PRINT_CODE == 2
  if (!parser.hadError) {
    disassembleChunk(currentChunk(), function->name != NULL ? function->name->chars : "main");
  }
# elif DEBUG_PRINT_CODE == 1
  if (!parser.hadError && !parser.module->isCore) {
    disassembleChunk(currentChunk(), function->name != NULL ? function->name->chars : "main");
  }
# endif

  while (current->loop != NULL) {
    if (current->loop->breaks != NULL) free(current->loop->breaks);
    current->loop = current->loop->enclosing;
  }

  current = current->enclosing;
  return function;
}

static void pushScope() { current->scopeDepth++; }

static int discardLocals(int depth) {
  ASSERT(current->scopeDepth >= 0, "Cannot exit top level scope");

  int local = current->localCount - 1;
  while (local >= 0 && current->locals[local].depth >= depth) {
    // To print out discarded locals:
    //
    // Token token = current->locals[local].name;
    // char* chars = ALLOCATE(char, token.length + 1);
    // memcpy(chars, token.start, token.length);
    // chars[token.length] = '\0';
    // printf("discarding %s (%d >= %d)\n", chars, current->locals[local].depth, depth);

    if (current->locals[local].isCaptured) {
      emitByte(OP_CLOSE_UPVALUE);
    } else {
      emitByte(OP_POP);
    }
    local--;
  }

  return current->localCount - local - 1;
}

static void popScope() {
  int popped = discardLocals(current->scopeDepth);
  current->localCount -= popped;
  current->scopeDepth--;
}

static void expression();
static void statement();
static void declaration();
static void lambda(bool canAssign);
static ParseRule* getRule(TokenType type);
static void expressionBp(BindingPower bp);

static int identifierConstant(Token* name) {
  return makeConstant(OBJ_VAL(copyStringLength(name->start, name->length)));
}

static bool identifiersEqual(Token* a, Token* b) {
  if (a->length != b->length) return false;
  return memcmp(a->start, b->start, a->length) == 0;
}

static int resolveLocal(Compiler* compiler, Token* name) {
  for (int i = compiler->localCount - 1; i >= 0; i--) {
    Local* local = &compiler->locals[i];
    if (identifiersEqual(name, &local->name)) {
      if (local->depth == -1) {
        error("Can't use local variable in its own initializer");
      }
      return i;
    }
  }

  return -1;
}

static int addUpvalue(Compiler* compiler, uint8_t index, bool isLocal) {
  int upvalueCount = compiler->function->upvalueCount;

  for (int i = 0; i < upvalueCount; i++) {
    Upvalue* upvalue = &compiler->upvalues[i];
    if (upvalue->index == index && upvalue->isLocal == isLocal) {
      return i;
    }
  }

  if (upvalueCount == UINT8_COUNT) {
    error("Too many closure variables in function");
    return 0;
  }

  compiler->upvalues[upvalueCount].isLocal = isLocal;
  compiler->upvalues[upvalueCount].index = index;
  return compiler->function->upvalueCount++;
}

static int resolveUpvalue(Compiler* compiler, Token* name) {
  if (compiler->enclosing == NULL) return -1;

  int local = resolveLocal(compiler->enclosing, name);
  if (local != -1) {
    compiler->enclosing->locals[local].isCaptured = true;
    return addUpvalue(compiler, (uint8_t)local, true);
  }

  int upvalue = resolveUpvalue(compiler->enclosing, name);
  if (upvalue != -1) {
    return addUpvalue(compiler, (uint8_t)upvalue, false);
  }

  return -1;
}

static void addLocal(Token name, bool isMutable) {
  if (current->localCount == UINT8_COUNT) {
    error("Too many local variables in one function");
    return;
  }

  Local* local = &current->locals[current->localCount++];
  local->name = name;
  local->depth = -1;
  local->isMutable = isMutable;
  local->isCaptured = false;
}

static void declareVariable(bool isMutable) {
  if (current->scopeDepth == 0) return;

  Token* name = &parser.previous;
  for (int i = current->localCount - 1; i >= 0; i--) {
    Local* local = &current->locals[i];
    if (local->depth != -1 && local->depth < current->scopeDepth) {
      break;
    }

    if (identifiersEqual(name, &local->name)) {
      error("Variable has been declared previously");
    }
  }

  addLocal(*name, isMutable);
}

static int parseVariable(const char* errorMessage, bool isMutable) {
  expect(TOKEN_IDENTIFIER, errorMessage);

  declareVariable(isMutable);
  if (current->scopeDepth > 0) return 0;

  return identifierConstant(&parser.previous);
}

static void markInitialized() {
  if (current->scopeDepth == 0) return;
  current->locals[current->localCount - 1].depth = current->scopeDepth;
}

static void defineVariable(int global, bool isMutable) {
  if (current->scopeDepth > 0) {
    markInitialized();
    return;
  }

  emitVariableArg(OP_DEFINE_IMMUTABLE_GLOBAL - isMutable, global);
}

static void signatureParameterList(char name[MAX_METHOD_SIGNATURE], int* length, int arity) {
  name[(*length)++] = '(';

  if (arity > 0) {
    int digits = ceil(log10(arity + 1));
    char* chars = ALLOCATE(char, digits);
    sprintf(chars, "%d", arity);

    memcpy(name + *length, chars, digits);
    *length += digits;

    FREE_ARRAY(char, chars, digits);
  }

  name[(*length)++] = ')';
}

static void signatureToString(Signature* signature, char name[MAX_METHOD_SIGNATURE], int* length) {
  *length = 0;

  memcpy(name, signature->name, signature->length);
  *length += signature->length;

  if (signature->type != SIG_ATTRIBUTE) {
    signatureParameterList(name, length, signature->arity);
  }

  name[*length] = '\0';
}

static Signature signatureFromToken(SignatureType type) {
  Signature signature;

  Token* token = &parser.previous;
  signature.name = token->start;
  signature.length = token->length;
  signature.type = type;
  signature.arity = 0;
  signature.asProperty = NULL;

  if (signature.length > MAX_METHOD_NAME) {
    error("Method names cannot be longer than %d characters", MAX_METHOD_NAME);
    signature.length = MAX_METHOD_NAME;
  }

  return signature;
}

static void validateParameterCount(const char* type, int num) {
  if (num == MAX_PARAMETERS + 1) {
    error("%ss cannot have more than %d parameters", type, MAX_PARAMETERS);
  }
}

static void finishParameterList(Signature* signature) {
  signature->asProperty = ALLOCATE(bool, MAX_PARAMETERS);
  do {
    matchLine();
    validateParameterCount("Method", ++signature->arity);

    signature->asProperty[signature->arity - 1] = match(TOKEN_PLUS);

    int constant = parseVariable("Expecting a parameter name", true);
    defineVariable(constant, true);
  } while (match(TOKEN_COMMA));
}

static void finishArgumentList(Signature* signature, const char* type, TokenType end) {
  if (!check(end)) {
    do {
      if (matchLine() && match(TOKEN_INDENT)) {
        parser.ignoreDedents++;
      }
      validateParameterCount(type, ++signature->arity);
      expression();
    } while (match(TOKEN_COMMA));

    matchLine();
  }

  if (end == TOKEN_RIGHT_PAREN) {
    expect(end, "Expecting ')' after arguments");
  } else {
    expect(end, (signature->arity == 1 ? "Expecting ']' after subscript value"
                                       : "Expecting ']' after subscript values"));
  }
}

static inline void emitSignatureArg(uint8_t instruction, Signature* signature) {
  char method[MAX_METHOD_SIGNATURE];
  int length;
  signatureToString(signature, method, &length);

  emitConstantArg(instruction, OBJ_VAL(copyStringLength(method, length)));
}

static inline void callSignature(int argCount, Signature* signature) {
  emitSignatureArg(OP_INVOKE_0 + argCount, signature);
}

void binarySignature(Signature* signature) {
  signature->type = SIG_METHOD;
  signature->arity = 1;
  signature->asProperty = NULL;

  expect(TOKEN_LEFT_PAREN, "Expecting '(' after operator name");
  int constant = parseVariable("Expecting a parameter name", true);
  defineVariable(constant, true);
  expect(TOKEN_RIGHT_PAREN, "Expecting ')' after parameter name");
}

void unarySignature(Signature* signature) {
  signature->type = SIG_METHOD;
  expect(TOKEN_LEFT_PAREN, "Expecting '(' after method name");
  expect(TOKEN_RIGHT_PAREN, "Expecting ')' after opening parenthesis");
}

void mixedSignature(Signature* signature) {
  signature->type = SIG_METHOD;

  if (match(TOKEN_LEFT_PAREN)) {
    signature->type = SIG_METHOD;
    signature->arity = 1;
    signature->asProperty = NULL;

    int constant = parseVariable("Expecting a parameter name", true);
    defineVariable(constant, true);
    expect(TOKEN_RIGHT_PAREN, "Expecting ')' after parameter name");
  }
}

void namedSignature(Signature* signature) {
  expect(TOKEN_LEFT_PAREN, "Expecting '(' after method name");

  signature->type = SIG_METHOD;

  matchLine();
  if (match(TOKEN_RIGHT_PAREN)) return;

  finishParameterList(signature);
  expect(TOKEN_RIGHT_PAREN, "Expecting ')' after parameters");
}

void attributeSignature(Signature* signature) {
  signature->type = SIG_ATTRIBUTE;
}

static Token syntheticToken(const char* text) {
  Token token;
  token.start = text;
  token.length = (int)strlen(text);
  return token;
}

static void namedVariable(Token name, bool canAssign) {
  uint8_t getOp, setOp;
  int arg = resolveLocal(current, &name);
  if (arg != -1) {
    getOp = OP_GET_LOCAL;
    setOp = OP_SET_LOCAL;
  } else if ((arg = resolveUpvalue(current, &name)) != -1) {
    getOp = OP_GET_UPVALUE;
    setOp = OP_SET_UPVALUE;
  } else {
    arg = identifierConstant(&name);
    getOp = OP_GET_GLOBAL;
    setOp = OP_SET_GLOBAL;
  }

  if (canAssign && match(TOKEN_EQ)) {
    if (setOp == OP_SET_LOCAL && !current->locals[arg].isMutable) {
      error("Value cannot be reassigned");
    }

    // bool modifier = false;
    // if (match(TOKEN_GT)) {
    //   if (getOp == OP_GET_GLOBAL) emitVariableArg(getOp, arg);
    //   else emitBytes(getOp, (uint8_t)arg);
    //   modifier = true;
    // }

    if (matchLine() && match(TOKEN_INDENT)) {
      parser.ignoreDedents++;
    }

    // if (modifier) {
    //   advance();
    //   ParseFn infixRule = getRule(parser.previous.type)->infix;
    //   if (infixRule == NULL) {
    //     error("Expecting an infix operator to modify variable");
    //   } else {
    //     infixRule(false);
    //   }
    // } else ...
    expression();

    if (setOp == OP_SET_GLOBAL) {
      emitVariableArg(setOp, arg);
    } else {
      emitBytes(setOp, (uint8_t)arg);
    }
  } else {
    if (getOp == OP_GET_GLOBAL) emitVariableArg(getOp, arg);
    else emitBytes(getOp, (uint8_t)arg);
  }
}

static void variable(bool canAssign) {
  namedVariable(parser.previous, canAssign);
}

static void call(bool canAssign) {
  Signature signature = { NULL, 0, SIG_METHOD, 0 };
  finishArgumentList(&signature, "Function", TOKEN_RIGHT_PAREN);
  emitByte(OP_CALL_0 + signature.arity);
}

static void callFunction(bool canAssign) {
  lambda(false);
  emitByte(OP_CALL_1);
}

static void callable(bool canAssign) {
  advance();
  ParseRule* rule = getRule(parser.previous.type);
  if (rule->signatureFn == NULL) {
    error("Expecting a method name after '::'");
  }

  Signature signature = signatureFromToken(SIG_METHOD);
  if (match(TOKEN_LEFT_PAREN)) {
    if (match(TOKEN_RIGHT_PAREN)) {
      signature.arity = 0;
    } else {
      expect(TOKEN_NUMBER, "Expecting a parameter count");
      double num = AS_NUMBER(parser.previous.value);
      signature.arity = (int)trunc(num);
      if (num != signature.arity) {
        error("Parameter count must be an integer");
      }
      expect(TOKEN_RIGHT_PAREN, "Expecting ')' after parameter count");
    }
  } else {
    signature.type = SIG_ATTRIBUTE;
  }

  emitSignatureArg(OP_BIND_METHOD, &signature);
}

static void dot(bool canAssign) {
  expect(TOKEN_IDENTIFIER, "Expecting a property name after '.'");

  int name = identifierConstant(&parser.previous);
  Signature signature = signatureFromToken(SIG_METHOD);

  if (canAssign && match(TOKEN_EQ)) {
    if (matchLine() && match(TOKEN_INDENT)) {
      parser.ignoreDedents++;
    }

    expression();
    emitVariableArg(OP_SET_PROPERTY, name);
  } else if (match(TOKEN_LEFT_PAREN) || match(TOKEN_LEFT_BRACE)) {
    if (parser.previous.type == TOKEN_LEFT_BRACE) {
      lambda(false);
      signature.arity = 1;
    } else {
      finishArgumentList(&signature, "Method", TOKEN_RIGHT_PAREN);
    }

    callSignature(signature.arity, &signature);
  } else {
    emitVariableArg(OP_GET_PROPERTY, name);
  }
}

static void super_(bool canAssign) {
  if (currentClass == NULL) {
    error("Can't use 'super' outside of a class");
  }

  Signature signature;
  uint8_t instruction;
  if (match(TOKEN_COLON_COLON)) {
    advance();
    ParseRule* rule = getRule(parser.previous.type);
    if (rule->signatureFn == NULL) {
      error("Expecting a method name after '::'");
    }

    signature = signatureFromToken(SIG_METHOD);

    if (match(TOKEN_LEFT_PAREN)) {
      if (match(TOKEN_RIGHT_PAREN)) {
        signature.arity = 0;
      } else {
        expect(TOKEN_NUMBER, "Expecting a parameter count");
        double num = AS_NUMBER(parser.previous.value);
        signature.arity = (int)trunc(num);
        if (num != signature.arity) {
          error("Parameter count must be an integer");
        }
        expect(TOKEN_RIGHT_PAREN, "Expecting ')' after parameter count");
      }
    } else {
      signature.type = SIG_ATTRIBUTE;
    }

    instruction = OP_BIND_SUPER;
  } else {
    expect(TOKEN_DOT, "Expecting '.' after 'super'");
    expect(TOKEN_IDENTIFIER, "Expecting a superclass method name");
    signature = signatureFromToken(SIG_METHOD);

    namedVariable(syntheticToken("this"), false);
    if (match(TOKEN_LEFT_PAREN) || match(TOKEN_LEFT_BRACE)) {
      if (parser.previous.type == TOKEN_LEFT_BRACE) {
        lambda(false);
        signature.arity = 1;
      } else {
        finishArgumentList(&signature, "Method", TOKEN_RIGHT_PAREN);
      }
    } else {
      signature.type = SIG_ATTRIBUTE;
    }

    namedVariable(syntheticToken("super"), false);
    instruction = OP_SUPER_0 + signature.arity;
  }

  emitSignatureArg(instruction, &signature);
}

static void this_(bool canAssign) {
  if (currentClass == NULL) {
    error("Can't use 'this' outside of a class");
    return;
  }
  if (current->type == TYPE_STATIC_METHOD) {
    error("Can't use 'this' in a static method");
    return;
  }

  namedVariable(parser.previous, false);
}

static void literal(bool canAssign) {
  switch (parser.previous.type) {
    case TOKEN_FALSE: emitByte(OP_FALSE); break;
    case TOKEN_NONE: emitByte(OP_NONE); break;
    case TOKEN_TRUE: emitByte(OP_TRUE); break;
    case TOKEN_NUMBER:
    case TOKEN_STRING: emitConstant(parser.previous.value);
    default: return; // Unreachable
  }
}

static void grouping(bool canAssign) {
  expression();
  expect(TOKEN_RIGHT_PAREN, "Expecting ')' after expressions");
}

static void or_(bool canAssign) {
  matchLine();

  int jump = emitJump(OP_JUMP_TRUTHY);

  emitByte(OP_POP);
  expressionBp(BP_OR);
  patchJump(jump);
}

static void and_(bool canAssign) {
  matchLine();

  int jump = emitJump(OP_JUMP_FALSY);

  emitByte(OP_POP);
  expressionBp(BP_AND);
  patchJump(jump);
}

static void if_(bool canAssign) {
  expression();

  int endJump = emitJump(OP_JUMP_TRUTHY_POP);

  expect(TOKEN_ELSE, "Expecting an else clause after condition");
  expression();

  patchJump(endJump);
}

static void stringInterpolation(bool canAssign) {
  emitConstantArg(OP_GET_GLOBAL, OBJ_VAL(copyStringLength("List", 4)));
  emitByte(OP_CALL_0);
  int addConstant = makeConstant(OBJ_VAL(copyStringLength("addCore(1)", 10)));

  do {
    emitConstant(parser.previous.value);
    emitVariableArg(OP_INVOKE_1, addConstant);

    matchLine();
    expression();
    emitVariableArg(OP_INVOKE_1, addConstant);

    matchLine();
  } while (match(TOKEN_INTERPOLATION));

  expect(TOKEN_STRING, "Expecting an end to string interpolation");
  emitConstant(parser.previous.value);
  emitVariableArg(OP_INVOKE_1, addConstant);

  emitConstant(OBJ_VAL(copyStringLength("", 0)));
  callMethod(1, "joinToString(1)", 15);
}

static void collection(bool canAssign) {
  if (match(TOKEN_RIGHT_BRACKET)) {
    emitConstantArg(OP_GET_GLOBAL, OBJ_VAL(copyStringLength("List", 4)));
    emitByte(OP_CALL_0);
    return;
  } else if (match(TOKEN_RIGHT_ARROW)) {
    expect(TOKEN_RIGHT_BRACKET, "Expecting ']' to end empty map");
    emitConstantArg(OP_GET_GLOBAL, OBJ_VAL(copyStringLength("Map", 3)));
    emitByte(OP_CALL_0);
    return;
  }

  bool indented = false;
  if (matchLine()) {
    expect(TOKEN_INDENT, "Expecting indentation to increase before collection body");
    indented = true;
  }

  ByteArray code;
  IntArray lines;
  byteArrayInit(&code);
  intArrayInit(&lines);

  current->customWrite = &code;
  current->customLine = &lines;

  expression();

  current->customWrite = NULL;
  current->customLine = NULL;

  bool isMap = match(TOKEN_RIGHT_ARROW);
  bool first = true;

  emitConstantArg(OP_GET_GLOBAL, isMap ? OBJ_VAL(copyStringLength("Map", 3))
                                       : OBJ_VAL(copyStringLength("List", 4)));
  emitByte(OP_CALL_0);

  do {
    if (matchLine()) {
      if (!indented) {
        expect(TOKEN_INDENT, "Expecting indentation to increase before collection body");
        indented = true;
      } else {
        if (match(TOKEN_DEDENT)) indented = false;
      }
    }

    if (check(TOKEN_RIGHT_BRACKET)) break;

    if (first) {
      // Transfer all the code that makes up the first item into the current chunk.
      for (int i = 0; i < code.count; i++) {
        writeChunk(currentChunk(), code.data[i], lines.data[i]);
      }

      byteArrayFree(&code);
      intArrayFree(&lines);
    } else {
      expression();
    }

    if (isMap) {
      if (!first) expect(TOKEN_RIGHT_ARROW, "Expecting '->' after map key");
      expression();
      callMethod(2, "addCore(2)", 10);
    } else {
      callMethod(1, "addCore(1)", 10);
    }

    first = false;
  } while (match(TOKEN_COMMA));

  matchLine();
  if (indented && match(TOKEN_DEDENT)) indented = false;
  expect(TOKEN_RIGHT_BRACKET, isMap ? "Expecting ']' after map literal" : "Expecting ']' after list literal");

  if (indented) {
    matchLine();
    expect(TOKEN_DEDENT, "Expecting indentation to decrease");
  }
}

static void subscript(bool canAssign) {
  Signature signature = { "get", 3, SIG_METHOD, 0, NULL };
  finishArgumentList(&signature, "Method", TOKEN_RIGHT_BRACKET);

  if (canAssign && match(TOKEN_EQ)) {
    if (matchLine() && match(TOKEN_INDENT)) {
      parser.ignoreDedents++;
    }

    expression();
    validateParameterCount("Method", ++signature.arity);
    signature.name = "set";
  }

  callSignature(signature.arity, &signature);
}

static void binary(bool canAssign) {
  TokenType operatorType = parser.previous.type;
  ParseRule* rule = getRule(operatorType);

  bool negate = (operatorType == TOKEN_IS && match(TOKEN_NOT));

  if (matchLine() && match(TOKEN_INDENT)) {
    parser.ignoreDedents++;
  }

  if (operatorType == TOKEN_STAR_STAR) {
    expressionBp((BindingPower)(rule->bp));
  } else {
    expressionBp((BindingPower)(rule->bp + 1));
  }

  Signature signature = { rule->name, (int)strlen(rule->name), SIG_METHOD, 1 };

  callSignature(1, &signature);
  if (negate) {
    callMethod(0, "not()", 5);
  }
}

static void unary(bool canAssign) {
  TokenType operatorType = parser.previous.type;
  ParseRule* rule = getRule(operatorType);

  if (matchLine() && match(TOKEN_INDENT)) {
    parser.ignoreDedents++;
  }

  // Compile the operand.
  expressionBp(operatorType == TOKEN_NOT ? BP_NOT : BP_UNARY);

  Signature signature = { rule->name, (int)strlen(rule->name), SIG_METHOD, 0 };

  callSignature(0, &signature);
}

#define UNUSED                    { NULL,   NULL,   BP_NONE, NULL, NULL }
#define INFIX(fn, bp)             { NULL,   fn,     bp,      NULL, NULL }
#define INFIX_OPERATOR(bp, name)  { NULL,   binary, bp,      name, binarySignature }
#define PREFIX(fn, bp)            { fn,     NULL,   bp,      NULL, NULL }
#define PREFIX_OPERATOR(bp, name) { unary,  NULL,   bp,      name, unarySignature }
#define BOTH(prefix, infix, bp)   { prefix, infix,  bp,      NULL, NULL }
#define OPERATOR(bp, name)        { unary,  binary, bp,      name, mixedSignature }

ParseRule rules[] = {
  /* TOKEN_LEFT_PAREN    */ BOTH(grouping, call, BP_CALL),
  /* TOKEN_RIGHT_PAREN   */ UNUSED,
  /* TOKEN_LEFT_BRACKET  */ BOTH(collection, subscript, BP_CALL),
  /* TOKEN_RIGHT_BRACKET */ UNUSED,
  /* TOKEN_LEFT_BRACE    */ BOTH(lambda, callFunction, BP_CALL),
  /* TOKEN_RIGHT_BRACE   */ UNUSED,
  /* TOKEN_SEMICOLON     */ UNUSED,
  /* TOKEN_COMMA         */ UNUSED,
  /* TOKEN_PLUS          */ INFIX_OPERATOR(BP_TERM, "+"),
  /* TOKEN_SLASH         */ INFIX_OPERATOR(BP_FACTOR, "/"),
  /* TOKEN_PERCENT       */ INFIX_OPERATOR(BP_FACTOR, "%"),
  /* TOKEN_PIPE          */ INFIX_OPERATOR(BP_BIT_OR, "|"),
  /* TOKEN_CARET         */ INFIX_OPERATOR(BP_BIT_XOR, "^"),
  /* TOKEN_AMPERSAND     */ INFIX_OPERATOR(BP_BIT_AND, "&"),
  /* TOKEN_TILDE         */ PREFIX_OPERATOR(BP_UNARY, "~"),
  /* TOKEN_DOT           */ INFIX(dot, BP_CALL),
  /* TOKEN_DOT_DOT       */ INFIX_OPERATOR(BP_RANGE, ".."),
  /* TOKEN_DOT_DOT_LT    */ INFIX_OPERATOR(BP_RANGE, "..<"),
  /* TOKEN_COLON         */ UNUSED,
  /* TOKEN_COLON_COLON   */ INFIX(callable, BP_CALL),
  /* TOKEN_STAR          */ INFIX_OPERATOR(BP_FACTOR, "*"),
  /* TOKEN_STAR_STAR     */ INFIX_OPERATOR(BP_EXPONENT, "**"),
  /* TOKEN_MINUS         */ OPERATOR(BP_TERM, "-"),
  /* TOKEN_RIGHT_ARROW   */ UNUSED,
  /* TOKEN_BANG          */ UNUSED,
  /* TOKEN_BANG_EQ       */ INFIX_OPERATOR(BP_EQUALITY, "!="),
  /* TOKEN_EQ            */ UNUSED,
  /* TOKEN_EQ_EQ         */ INFIX_OPERATOR(BP_EQUALITY, "=="),
  /* TOKEN_GT            */ INFIX_OPERATOR(BP_COMPARISON, ">"),
  /* TOKEN_GT_EQ         */ INFIX_OPERATOR(BP_COMPARISON, ">="),
  /* TOKEN_LT            */ INFIX_OPERATOR(BP_COMPARISON, "<"),
  /* TOKEN_LT_EQ         */ INFIX_OPERATOR(BP_COMPARISON, "<="),
  /* TOKEN_IDENTIFIER    */ { variable, NULL, BP_NONE, NULL, namedSignature },
  /* TOKEN_STRING        */ PREFIX(literal, BP_NONE),
  /* TOKEN_INTERPOLATION */ PREFIX(stringInterpolation, BP_NONE),
  /* TOKEN_NUMBER        */ PREFIX(literal, BP_NONE),
  /* TOKEN_AND           */ INFIX(and_, BP_AND),
  /* TOKEN_ATTRIBUTE     */ UNUSED,
  /* TOKEN_BREAK         */ UNUSED,
  /* TOKEN_CLASS         */ UNUSED,
  /* TOKEN_CONTINUE      */ UNUSED,
  /* TOKEN_DO            */ UNUSED,
  /* TOKEN_EACH          */ UNUSED,
  /* TOKEN_ELIF          */ UNUSED,
  /* TOKEN_ELSE          */ UNUSED,
  /* TOKEN_FALSE         */ PREFIX(literal, BP_NONE),
  /* TOKEN_FOR           */ UNUSED,
  /* TOKEN_FUN           */ UNUSED,
  /* TOKEN_IF            */ INFIX(if_, BP_IF),
  /* TOKEN_IN            */ UNUSED,
  /* TOKEN_IS            */ INFIX_OPERATOR(BP_IS, "is"),
  /* TOKEN_NONE          */ PREFIX(literal, BP_NONE),
  /* TOKEN_NOT           */ PREFIX_OPERATOR(BP_NOT, "not"),
  /* TOKEN_OR            */ INFIX(or_, BP_OR),
  /* TOKEN_PASS          */ UNUSED,
  /* TOKEN_PRINT         */ UNUSED,
  /* TOKEN_PRINT_ERROR   */ UNUSED,
  /* TOKEN_RETURN        */ UNUSED,
  /* TOKEN_SHL           */ INFIX_OPERATOR(BP_BIT_SHIFT, "shl"),
  /* TOKEN_SHR           */ INFIX_OPERATOR(BP_BIT_SHIFT, "shr"),
  /* TOKEN_STATIC        */ UNUSED,
  /* TOKEN_SUPER         */ PREFIX(super_, BP_NONE),
  /* TOKEN_THIS          */ PREFIX(this_, BP_NONE),
  /* TOKEN_TRUE          */ PREFIX(literal, BP_NONE),
  /* TOKEN_USE           */ UNUSED,
  /* TOKEN_VAL           */ UNUSED,
  /* TOKEN_VAR           */ UNUSED,
  /* TOKEN_WHEN          */ UNUSED, // TODO: When expressions
  /* TOKEN_WHILE         */ UNUSED,
  /* TOKEN_INDENT        */ UNUSED,
  /* TOKEN_DEDENT        */ UNUSED,
  /* TOKEN_LINE          */ UNUSED,
  /* TOKEN_ERROR         */ UNUSED,
  /* TOKEN_EOF           */ UNUSED,
  /* TOKEN_NULL          */ UNUSED, // The compiler should never see a null token.
};

static void expressionBp(BindingPower bp) {
  advance();
  ParseFn prefixRule = getRule(parser.previous.type)->prefix;
  if (prefixRule == NULL) {
    error("Expecting an expression");
    return;
  }

  bool canAssign = bp <= BP_ASSIGNMENT;
  prefixRule(canAssign);

  while (bp <= getRule(parser.current.type)->bp) {
    advance();
    ParseFn infixRule = getRule(parser.previous.type)->infix;
    infixRule(canAssign);
  }

  // The equals sign should be handled already.
  if (canAssign && match(TOKEN_EQ)) {
    error("Invalid assignment target");
  }
}

static inline ParseRule* getRule(TokenType type) { return &rules[type]; }

static void expression() { expressionBp(BP_ASSIGNMENT); }

// indentationBased is to help with this weird case:
//
// if (variable == 2) {
// if (otherVar == 4)
//   print "yes"
// }

static void block() {
  matchLine();

  while (!check(TOKEN_DEDENT) && !check(TOKEN_EOF)) {
    declaration();

    if (!check(TOKEN_EOF)) {
      expectStatementEnd("Expecting a newline after statement");
    }

    matchLine();
  }

  if (!check(TOKEN_EOF)) expect(TOKEN_DEDENT, "Expecting indentation to decrease after block");
}

static void lambdaBlock() {
  if (matchLine()) ignoreIndentation();
  parser.onExpression = false;

  while (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF)) {
    if (parser.onExpression) {
      emitByte(OP_POP);
      parser.onExpression = false;
    }

    declaration();

    if (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF)) {
      expectStatementEnd("Expecting a newline after statement");
      ignoreIndentation();
    }
  }

  expect(TOKEN_RIGHT_BRACE, "Expecting '}' after lambda");
}

static void scopedBlock() {
  pushScope();
  block();
  popScope();
}

static void function(FunctionType type) {
  Compiler compiler;
  initCompiler(&compiler, type);
  pushScope();

  expect(TOKEN_LEFT_PAREN, "Expecting '(' after function name");
  if (!check(TOKEN_RIGHT_PAREN)) {
    do {
      matchLine(); // TODO: We almost always want function parameters indented.
      validateParameterCount("Function", ++current->function->arity);

      int constant = parseVariable("Expecting a parameter name", true);
      defineVariable(constant, true);
    } while (match(TOKEN_COMMA));
  }
  expect(TOKEN_RIGHT_PAREN, "Expecting ')' after parameters");

  if (match(TOKEN_EQ)) {
    expression();
    emitByte(OP_RETURN);
  } else {
    expectLine("Expecting a linebreak before function body");
    expect(TOKEN_INDENT, "Expecting an indent before function body");
    block();
  }

  ObjFunction* function = endCompiler();
  emitConstantArg(OP_CLOSURE, OBJ_VAL(function));

  for (int i = 0; i < function->upvalueCount; i++) {
    emitByte(compiler.upvalues[i].isLocal ? 1 : 0);
    emitByte(compiler.upvalues[i].index);
  }
}

static void lambda(bool canAssign) {
  Compiler compiler;
  initCompiler(&compiler, TYPE_LAMBDA);
  pushScope();

  if (matchLine()) ignoreIndentation();

  if (match(TOKEN_PIPE)) {
    if (!match(TOKEN_PIPE)) {
      do {
        if (matchLine()) ignoreIndentation();
        validateParameterCount("Lambda", ++current->function->arity);

        int constant = parseVariable("Expecting a parameter name", true);
        defineVariable(constant, true);
      } while (match(TOKEN_COMMA));
      expect(TOKEN_PIPE, "Expecting '|' after parameters");
      if (matchLine()) ignoreIndentation();
    }
  }

  lambdaBlock();

  // If the lambda is just an expression, return its value.
  if (parser.onExpression) {
    emitByte(OP_RETURN);
    parser.onExpression = false;
  }

  ObjFunction* function = endCompiler();
  emitConstantArg(OP_CLOSURE, OBJ_VAL(function));

  for (int i = 0; i < function->upvalueCount; i++) {
    emitByte(compiler.upvalues[i].isLocal ? 1 : 0);
    emitByte(compiler.upvalues[i].index);
  }

  if ((parser.printResult && current->scopeDepth == 0) || current->type == TYPE_LAMBDA) {
    parser.onExpression = true;
  }
}

static void method() {
  bool isStatic = match(TOKEN_STATIC);
  bool isAttribute = match(TOKEN_ATTRIBUTE);

  if (isAttribute && match(TOKEN_STATIC)) error("Keyword 'static' must come before 'attribute'");

  SignatureFn parseSignature = isAttribute ?
              attributeSignature : getRule(parser.current.type)->signatureFn;
  advance();

  if (parseSignature == NULL) {
    error("Expecting a method definition");
    return;
  }

  Signature signature = signatureFromToken(SIG_METHOD);

  FunctionType type = isStatic ? TYPE_STATIC_METHOD : TYPE_METHOD;
  if (parser.previous.length == 4 && memcmp(parser.previous.start, "init", 4) == 0) {
    if (isStatic) error("Initializers cannot be static");
    if (currentClass->hasInitializer) error("Classes can only have one initializer");

    type = TYPE_INITIALIZER;
    currentClass->hasInitializer = true;
  }

  Compiler compiler;
  initCompiler(&compiler, type);
  pushScope();

  parseSignature(&signature);

  current->function->arity = signature.arity;

  if (signature.asProperty != NULL) {
    for (int i = 0; i < current->function->arity; i++) {
      if (signature.asProperty[i]) {
        if (isStatic) {
          error("Can only store fields through non-static methods");
          break;
        }
        emitBytes(OP_GET_LOCAL, 0);
        emitBytes(OP_GET_LOCAL, i + 1);
        Local local = current->locals[i + 1];
        emitVariableArg(OP_SET_PROPERTY, identifierConstant(&local.name));
        emitByte(OP_POP);
      }
    }
    FREE_ARRAY(bool, signature.asProperty, MAX_PARAMETERS);
  }

  if (match(TOKEN_EQ)) {
    if (current->type == TYPE_INITIALIZER) {
      error("Can't return a value from an initializer");
      emitBytes(OP_GET_LOCAL, 0);
    } else {
      expression();
    }
    emitByte(OP_RETURN);
  } else {
    expectLine("Expecting a linebreak before method body");
    expect(TOKEN_INDENT, "Expecting an indent before method body");
    block();
  }

  ObjFunction* result = endCompiler();
  emitConstantArg(OP_CLOSURE, OBJ_VAL(result));

  for (int i = 0; i < result->upvalueCount; i++) {
    emitByte(compiler.upvalues[i].isLocal ? 1 : 0);
    emitByte(compiler.upvalues[i].index);
  }

  if (type == TYPE_INITIALIZER) {
    emitByte(OP_INITIALIZER);
  } else {
    emitSignatureArg(OP_METHOD_INSTANCE + isStatic, &signature);
  }
}

static void classDeclaration() {
  expect(TOKEN_IDENTIFIER, "Expecting a class name");
  Token className = parser.previous;
  int nameConstant = identifierConstant(&parser.previous);
  declareVariable(false);

  if (match(TOKEN_LT)) {
    expect(TOKEN_IDENTIFIER, "Expecting a superclass name");
    variable(false);

    if (identifiersEqual(&className, &parser.previous)) {
      error("A class can't inherit from itself");
    }
  } else {
    emitConstantArg(OP_GET_GLOBAL, OBJ_VAL(copyString("Object")));
  }

  emitVariableArg(OP_CLASS, nameConstant);
  defineVariable(nameConstant, false);

  ClassCompiler classCompiler;
  classCompiler.hasInitializer = false;
  classCompiler.enclosing = currentClass;
  currentClass = &classCompiler;

  pushScope();
  addLocal(syntheticToken("super"), false);
  defineVariable(0, false);

  namedVariable(className, false);

  bool empty = match(TOKEN_LEFT_BRACKET) && match(TOKEN_RIGHT_BRACKET);
  empty = empty || match(TOKEN_SEMICOLON);
  if (!empty) {
    expectLine("Expecting a linebreak before class body");
    expect(TOKEN_INDENT, "Expecting an indent before class body");

    matchLine();

    while (!check(TOKEN_DEDENT) && !check(TOKEN_EOF)) {
      method();
      matchLine();
    }

    if (!check(TOKEN_EOF)) expect(TOKEN_DEDENT, "Expecting indentation to decrease after class body");
  }

  emitByte(OP_POP); // Class
  popScope();

  currentClass = currentClass->enclosing;
}

static void funDeclaration() {
  int global = parseVariable("Expecting a function name", true);
  markInitialized();
  function(TYPE_FUNCTION);
  defineVariable(global, true);
}

static void varDeclaration() {
  bool isMutable = (parser.previous.type == TOKEN_VAR);
  if (match(TOKEN_LEFT_PAREN)) {
    IntArray vars;
    intArrayInit(&vars);

    do {
      intArrayWrite(&vars, parseVariable("Expecting a variable name", isMutable));
    } while (match(TOKEN_COMMA));

    if (vars.count > UINT8_MAX) {
      error("Cannot define more than %d variables with one statement", UINT8_MAX);
    }

    expect(TOKEN_RIGHT_PAREN, "Expecting ')' after variable names");

    bool isNone = false;
    if (match(TOKEN_EQ)) {
      if (matchLine() && match(TOKEN_INDENT)) {
        parser.ignoreDedents++;
      }
      expression();
    } else {
      isNone = true;
      emitByte(OP_NONE);
    }

    // TODO: If the variable is immutable and not initialized, emit a compiler warning.

    if (!isNone) {
      emitByte(OP_DUP);
      callMethod(0, "count", 5);
      emitConstant(NUMBER_VAL((uint8_t)vars.count));
      callMethod(1, "==(1)", 5);
      int jump = emitJump(OP_JUMP_TRUTHY_POP);

      if (vars.count == 1) {
        emitConstant(OBJ_VAL(copyStringLength("Must have exactly 1 value to unpack", 35)));
      } else {
        emitConstant(OBJ_VAL(stringFormat("Must have exactly $ values to unpack", numberToCString(vars.count))));
      }
      emitByte(OP_ERROR);

      patchJump(jump);
    }

    for (int i = 0; i < vars.count; i++) {
      emitByte(OP_DUP);
      if (!isNone) {
        emitConstant(NUMBER_VAL((uint8_t)i));
        callMethod(1, "get(1)", 6);
      }
      defineVariable(vars.data[i], isMutable);
    }

    intArrayFree(&vars);
    emitByte(OP_POP); // The value was duplicated, so pop the original.
  } else {
    int global = parseVariable("Expecting a variable name", isMutable);

    if (match(TOKEN_EQ)) {
      if (matchLine() && match(TOKEN_INDENT)) parser.ignoreDedents++;
      expression();
    } else {
      emitByte(OP_NONE);
    }

    defineVariable(global, isMutable);
  }
}

static void useStatement() {
  int variableCount = 0;
  IntArray sourceConstants;
  IntArray nameConstants;

  if (!check(TOKEN_STRING)) {
    intArrayInit(&sourceConstants);
    intArrayInit(&nameConstants);
    do {
      matchLine();

      expect(TOKEN_IDENTIFIER, "Expecting a variable name");

      variableCount++;
      int sourceConstant = identifierConstant(&parser.previous);
      int nameConstant = sourceConstant;
      if (match(TOKEN_RIGHT_ARROW)) {
        nameConstant = parseVariable("Expecting a variable name alias", false);
      } else {
        declareVariable(false);
      }

      intArrayWrite(&sourceConstants, sourceConstant);
      intArrayWrite(&nameConstants, nameConstant);
    } while (match(TOKEN_COMMA));

    expect(TOKEN_IDENTIFIER, "Expecting 'from' after import variables");
    if (parser.previous.length != 4 || memcmp(parser.previous.start, "from", 4) != 0) {
      error("Expecting 'from' after import variables");
    }
  }

  expect(TOKEN_STRING, "Expecting a module to import");
  emitConstantArg(OP_IMPORT_MODULE, parser.previous.value);
  // Pop the return value from the module
  emitByte(OP_POP);

  if (variableCount > 0) {
    for (int i = 0; i < variableCount; i++) {
      emitVariableArg(OP_IMPORT_VARIABLE, sourceConstants.data[i]);
      defineVariable(nameConstants.data[i], false);
    }
  }
}

static void expressionStatement() {
  expression();
  emitByte(OP_POP);
}

static void printStatement() {
  expression();
  callMethod(0, "toString()", 10);
  emitByte(OP_PRINT);
}

static void errorStatement() {
  expression();
  callMethod(0, "toString()", 10);
  emitByte(OP_ERROR);
}

static void breakStatement() {
  if (current->loop == NULL) {
    error("Can't use 'break' outside of a loop");
  }

  Token* label = NULL;
  if (match(TOKEN_COLON)) {
    expect(TOKEN_IDENTIFIER, "Expecting a label after ':'");
    label = ALLOCATE(Token, 1);
    memcpy(label, &parser.previous, sizeof(Token));
  }

  Loop* loop = current->loop;

  if (label != NULL) {
    for (;;) {
      if (loop->label != NULL && identifiersEqual(loop->label, label)) break;

      loop = loop->enclosing;
      if (loop == NULL) {
        error("Can't find loop with this label");
        return;
      }
    }
  }

  discardLocals(loop->scopeDepth + 1);

  if (loop->breakCapacity < loop->breakCount + 1) {
    loop->breakCapacity = GROW_CAPACITY(loop->breakCapacity);
    loop->breaks = (int*)realloc(loop->breaks, sizeof(int) * loop->breakCapacity);
  }

  loop->breaks[loop->breakCount++] = emitJump(OP_JUMP);

  FREE(Token, label);
}

static void continueStatement() {
  if (current->loop == NULL) {
    error("Can't use 'continue' outside of a loop");
  }

  Token* label = NULL;
  if (match(TOKEN_COLON)) {
    expect(TOKEN_IDENTIFIER, "Expecting a label after ':'");
    label = ALLOCATE(Token, 1);
    memcpy(label, &parser.previous, sizeof(Token));
  }

  Loop* loop = current->loop;

  if (label != NULL) {
    for (;;) {
      if (loop->label != NULL && identifiersEqual(loop->label, label)) break;

      loop = loop->enclosing;
      if (loop == NULL) {
        error("Can't find loop with this label");
        return;
      }
    }
  }

  discardLocals(loop->scopeDepth + 1);

  // Jump to the top of the loop.
  emitLoop(loop->start);

  FREE(Token, label);
}

static void returnStatement() {
  if (check(TOKEN_LINE) || check(TOKEN_SEMICOLON)) {
    emitReturn();
  } else {
    if (current->type == TYPE_INITIALIZER) {
      error("Can't return a value from an initializer");
    }

    int values = 0;
    do {
      values++;
      expression();
      if (matchLine() && match(TOKEN_INDENT)) parser.ignoreDedents++;
    } while (match(TOKEN_COMMA));

    if (values >= UINT8_MAX) {
      error("Cannot return more than %d values", UINT8_MAX);
    }

    if (values > 1) emitBytes(OP_TUPLE, (uint8_t)values);
    emitByte(OP_RETURN);
  }
}

static void whileStatement() {
  Token* label = NULL;
  if (match(TOKEN_COLON)) {
    expect(TOKEN_IDENTIFIER, "Expecting a loop label");
    label = ALLOCATE(Token, 1);
    memcpy(label, &parser.previous, sizeof(Token));
  }

  Loop loop;
  startLoop(&loop);

  current->loop->label = label;

  expression(); // Condition

  current->loop->exitJump = emitJump(OP_JUMP_FALSY);
  emitByte(OP_POP);

  if (match(TOKEN_DO) && !check(TOKEN_LINE)) {
    statement();
  } else {
    expectLine("Expecting a linebreak after condition");
    expect(TOKEN_INDENT, "Expecting an indent before body");
    scopedBlock();
  }

  endLoop();

  FREE(Token, label);
}

static void forStatement() {
  pushScope();

  Token* label = NULL;
  if (match(TOKEN_COLON)) {
    expect(TOKEN_IDENTIFIER, "Expecting a loop label");
    label = ALLOCATE(Token, 1);
    memcpy(label, &parser.previous, sizeof(Token));
  }

  if (match(TOKEN_SEMICOLON)) {
    // No initializer.
  } else if (match(TOKEN_VAR) || match(TOKEN_VAL)) {
    varDeclaration();
    expectStatementEnd("Expecting ';' after loop initializer");
  } else {
    expressionStatement();
    expectStatementEnd("Expecting ';' after loop initializer");
  }

  Loop loop;
  startLoop(&loop);

  current->loop->label = label;

  current->loop->exitJump = -1;
  if (!match(TOKEN_SEMICOLON)) {
    expression();
    expectStatementEnd("Expecting ';' after loop condition");

    // Jump out of the loop if the condition is false.
    current->loop->exitJump = emitJump(OP_JUMP_FALSY);
    emitByte(OP_POP);
  }

  if (!check(TOKEN_DO) && !check(TOKEN_LINE)) {
    int bodyJump = emitJump(OP_JUMP);
    int incrementStart = currentChunk()->count;
    expressionStatement();

    emitLoop(current->loop->start);
    current->loop->start = incrementStart;
    patchJump(bodyJump);
  }

  if (match(TOKEN_DO) && !check(TOKEN_LINE)) {
    statement();
  } else {
    expectLine("Expecting a linebreak after condition");
    expect(TOKEN_INDENT, "Expecting an indent before body");
    scopedBlock();
  }

  endLoop();
  popScope();

  FREE(Token, label);
}

static void eachStatement() {
  pushScope(); // Scope for hidden iterator variables

  expect(TOKEN_IDENTIFIER, "Expecting a loop variable");
  Token name = parser.previous;

  Token index;
  bool hasIndex = false;
  if (match(TOKEN_LEFT_BRACKET)) {
    hasIndex = true;
    expect(TOKEN_IDENTIFIER, "Expecting an index variable");
    index = parser.previous;
    expect(TOKEN_RIGHT_BRACKET, "Expecting ']' after index variable");
  }

  expect(TOKEN_IN, "Expecting 'in' after loop variable");
  matchLine();

  expression();

  if (current->localCount + 2 > UINT8_COUNT) {
    error("Cannot declare any more locals.");
    return;
  }

  addLocal(syntheticToken("`seq"), false);
  markInitialized();
  int seqSlot = current->localCount - 1;
  emitByte(OP_NONE);
  addLocal(syntheticToken("`iter"), false);
  markInitialized();
  int iterSlot = current->localCount - 1;

  Loop loop;
  startLoop(&loop);

  emitBytes(OP_GET_LOCAL, seqSlot);
  emitBytes(OP_GET_LOCAL, iterSlot);

  callMethod(1, "iterate(1)", 10);
  emitBytes(OP_SET_LOCAL, iterSlot);

  current->loop->exitJump = emitJump(OP_JUMP_FALSY);

  emitByte(OP_POP);
  emitBytes(OP_GET_LOCAL, seqSlot);
  emitBytes(OP_GET_LOCAL, iterSlot);
  callMethod(1, "iteratorValue(1)", 16);

  pushScope(); // Loop variable
  addLocal(name, false);
  markInitialized();
  if (hasIndex) {
    emitBytes(OP_GET_LOCAL, iterSlot);
    addLocal(index, false);
    markInitialized();
  }

  if (match(TOKEN_DO) && !check(TOKEN_LINE)) {
    statement();
  } else {
    expectLine("Expecting a linebreak after condition");
    expect(TOKEN_INDENT, "Expecting an indent before body");
    block();
  }

  popScope(); // Loop variable

  endLoop();

  popScope(); // `seq and `iter variables
}

static void ifStatement() {
  expression();

  int thenJump = emitJump(OP_JUMP_FALSY);
  emitByte(OP_POP);

  if (match(TOKEN_DO) && !check(TOKEN_LINE)) {
    statement();
  } else {
    expectLine("Expecting a linebreak after condition");
    expect(TOKEN_INDENT, "Expecting an indent before body");
    scopedBlock();
  }

  int elseJump = emitJump(OP_JUMP);

  patchJump(thenJump);
  emitByte(OP_POP);

  if (match(TOKEN_ELIF)) ifStatement();
  else if (match(TOKEN_ELSE)) {
    if (match(TOKEN_DO) && !check(TOKEN_LINE)) {
      statement();
    } else {
      expectLine("Expecting a linebreak after 'else'");
      expect(TOKEN_INDENT, "Expecting an indent before body");
      scopedBlock();
    }
  }
  patchJump(elseJump);
}

#define MAX_WHEN_CASES 256

// TODO: Review when statements, the code is old and unpolished.
static void whenStatement() {
  expression();
  match(TOKEN_DO);

  // TODO: Possibly make custom operators, like this:
  //
  // when var
  //   >= 3 do something()
  //   == 3 do somethingElse()
  //   3 do somethingElse() # same as ==

  expectLine("Expecting a newline before cases");
  expect(TOKEN_INDENT, "Expecting an indent before cases");
  matchLine();

  int state = 0; // 0 at very start, 1 before default, 2 after default
  int caseEnds[MAX_WHEN_CASES];
  int caseCount = 0;
  int previousCaseSkip = -1;

  if (check(TOKEN_DEDENT)) errorAtCurrent("When statement must have at least one case");

  while (!check(TOKEN_DEDENT) && !check(TOKEN_EOF)) {
    if (match(TOKEN_IS) || match(TOKEN_ELSE)) {
      TokenType caseType = parser.previous.type;

      if (state == 2) {
        error("Can't have any cases after the default case");
      }

      if (state == 1) {
        if (caseCount == MAX_WHEN_CASES) {
          error("When statements cannot have more than %d cases", MAX_WHEN_CASES);
        }

        caseEnds[caseCount++] = emitJump(OP_JUMP);

        patchJump(previousCaseSkip);
        emitByte(OP_POP);
      }

      if (caseType == TOKEN_IS) {
        state = 1;

        emitByte(OP_DUP);
        expression();

        // TODO: Add multiple expressions to compare to, like this:
        //
        // when var
        //   is 3 | 4 do print "3 or 4"

        callMethod(1, "==(1)", 5);
        previousCaseSkip = emitJump(OP_JUMP_FALSY);

        // Pop the comparison result
        emitByte(OP_POP);

        if (match(TOKEN_DO) && !check(TOKEN_LINE)) {
          statement();
          expectLine("Expecting a newline after statement");
        } else {
          expectLine("Expecting a linebreak after case");
          expect(TOKEN_INDENT, "Expecting an indent before body");
          scopedBlock();
        }
      } else {
        if (state == 0) {
          error("Can't have a default case first");
        }
        state = 2;
        previousCaseSkip = -1;

        if (match(TOKEN_DO) && !check(TOKEN_LINE)) {
          statement();
          expectLine("Expecting a newline after statement");
        } else {
          expectLine("Expecting a linebreak after condition");
          expect(TOKEN_INDENT, "Expecting an indent before body");
          scopedBlock();
        }
      }
    } else {
      if (state == 0) {
        error("Can't have statements before any case");
      }
      statement();
      if (!check(TOKEN_EOF)) expectStatementEnd("Expecting a newline after statement");
    }
  }

  if (!check(TOKEN_EOF)) expect(TOKEN_DEDENT, "Expecting indentation to decrease after cases");

  // If there is no default case, patch the jump.
  if (state == 1) {
    patchJump(previousCaseSkip);
    emitByte(OP_POP);
  }

  for (int i = 0; i < caseCount; i++) {
    patchJump(caseEnds[i]);
  }

  emitByte(OP_POP); // The switch value
}

static void synchronize() {
  parser.panicMode = false;

  while (parser.current.type != TOKEN_EOF) {
    if (parser.previous.type == TOKEN_SEMICOLON) return;
    switch (parser.current.type) {
      case TOKEN_CLASS:
      case TOKEN_ATTRIBUTE:
      case TOKEN_STATIC:
      case TOKEN_FUN:
      case TOKEN_VAR:
      case TOKEN_FOR:
      case TOKEN_EACH:
      case TOKEN_WHILE:
      case TOKEN_WHEN:
      case TOKEN_BREAK:
      case TOKEN_CONTINUE:
      case TOKEN_PRINT:
      case TOKEN_PRINT_ERROR:
      case TOKEN_RETURN:
        return;

      default:; // Do nothing.
    }

    advance();
  }
}

static void declaration() {
  if (match(TOKEN_CLASS)) classDeclaration();
  else if (match(TOKEN_FUN)) funDeclaration();
  else if (match(TOKEN_VAR) || match(TOKEN_VAL)) varDeclaration();
  else if (match(TOKEN_USE)) useStatement();
  else statement();

  if (parser.panicMode) synchronize();
}

static void statement() {
  if (match(TOKEN_PRINT)) printStatement();
  else if (match(TOKEN_PRINT_ERROR)) errorStatement();
  else if (match(TOKEN_PASS)) return;
  else if (match(TOKEN_BREAK)) breakStatement();
  else if (match(TOKEN_CONTINUE)) continueStatement();
  else if (match(TOKEN_RETURN)) returnStatement();
  else if (match(TOKEN_WHILE)) whileStatement();
  else if (match(TOKEN_FOR)) forStatement();
  else if (match(TOKEN_EACH)) eachStatement();
  else if (match(TOKEN_IF)) ifStatement();
  else if (match(TOKEN_WHEN)) whenStatement();
  else {
    if ((parser.printResult && current->scopeDepth == 0) || current->type == TYPE_LAMBDA) {
      parser.onExpression = true;
      expression();
    } else expressionStatement();
  }
}

ObjFunction* compile(const char* source, ObjModule* module, bool printResult) {
  parser.module = module;
  parser.printResult = printResult;
  parser.hadError = false;
  parser.panicMode = false;
  parser.onExpression = false;

  parser.ignoreDedents = 0;

  initLexer(source);
  Compiler compiler;
  initCompiler(&compiler, TYPE_SCRIPT);

  advance();

# if DEBUG_PRINT_TOKENS == 1
  if (!module->isCore) {
    do {
      printf("%d\n", parser.current.type);
      advance();
    } while (!match(TOKEN_EOF));

    return NULL;
  }
# elif DEBUG_PRINT_TOKENS == 2
  do {
    printf("%d\n", parser.current.type);
    advance();
  } while (!match(TOKEN_EOF));

  return NULL;
# endif

  matchLine();
  if (match(TOKEN_INDENT)) error("Unexpected indentation");

  while (!match(TOKEN_EOF)) {
    if (parser.onExpression) {
      emitByte(OP_POP);
      parser.onExpression = false;
    }
    declaration();

    if (parser.previous.type != TOKEN_DEDENT) {
      if (!match(TOKEN_SEMICOLON) && !matchLine()) {
        matchLine();
        expect(TOKEN_EOF, "Expecting end of file");
        break;
      }

      matchLine();
    }
  }

  emitByte(OP_END_MODULE);

  ObjFunction* function = endCompiler();
  return parser.hadError ? NULL : function;
}

void markCompilerRoots() {
  markObject((Obj*)parser.module);
  Compiler* compiler = current;
  while (compiler != NULL) {
    markObject((Obj*)compiler->function);
    compiler = compiler->enclosing;
  }
}
