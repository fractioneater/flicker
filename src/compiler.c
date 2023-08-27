#include "compiler.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "lexer.h"
#include "memory.h"

#if DEBUG_PRINT_CODE
#include "debug.h"
#endif

typedef struct {
  // The newest token, fresh from the lexer.
  Token current;

  // The most recently consumed token.
  Token previous;

  // The name of the module being parsed.
  const char* module;

  // If a compiler error has appeared.
  bool hadError;
  bool panicMode;
} Parser;

typedef enum {
  BP_NONE,
  BP_ASSIGNMENT,  // =
  BP_NOT,         // not
  BP_OR,          // or
  BP_AND,         // and
  BP_EQUALITY,    // == !=
  BP_COMPARISON,  // < > <= >=
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
  SIG_GETTER,
  SIG_INITIALIZER
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
#if METHOD_CALL_OPERATORS
  const char* name;
  SignatureFn signatureFn;
#endif
} ParseRule;

typedef struct {
  // The name of the local variable.
  Token name;

  // The scope depth that this variable was declared in.
  int depth;

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

  // The level of block scope nesting.
  int scopeDepth;
} Compiler;

typedef struct ClassCompiler {
  struct ClassCompiler* enclosing;
  bool hasSuperclass;
} ClassCompiler;

Parser parser;
Compiler* current = NULL;
ClassCompiler* currentClass = NULL;

static inline Chunk* currentChunk() {
  return &current->function->chunk;
}

static void errorAt(Token* token, const char* message) {
  if (parser.panicMode) return;
  parser.panicMode = true;
  fprintf(stderr, "\033[1m%s:%d:\033[0m ", parser.module, token->line);

  if (token->type == TOKEN_EOF) {
    fprintf(stderr, "error at end");
  } else if (token->type == TOKEN_LINE) {
    fprintf(stderr, "error at newline");
  } else if (token->type == TOKEN_INDENT || token->type == TOKEN_DEDENT) {
    fprintf(stderr, "indentation error");
  } else if (token->type == TOKEN_ERROR) {
    fprintf(stderr, "error");
  } else {
    fprintf(stderr, "error at '%.*s'", token->length, token->start);
  }

  fprintf(stderr, ": %s\n", message);
  parser.hadError = true;
}

static void error(const char* message) {
  errorAt(&parser.previous, message);
}

static void errorAtCurrent(const char* message) {
  errorAt(&parser.current, message);
}

static void advance() {
  parser.previous = parser.current;

  for (;;) {
    parser.current = scanToken();
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

static bool ignoreIndentation() {
  if (!match(TOKEN_INDENT)) {
    if (!match(TOKEN_DEDENT)) return false;
    while (match(TOKEN_DEDENT));
  }
  return true;
}

static void expectStatementEnd(const char* message) {
  if (matchLine()) return;
  if (!match(TOKEN_SEMICOLON)) {
    errorAtCurrent(message);
  }
  matchLine();
}

static void emitByte(uint8_t byte) {
  writeChunk(currentChunk(), byte, parser.previous.line);
}

// Uncomment if needed, delete if not needed
// static void emitShort(int arg) {
//   emitByte((arg >> 8) & 0xff);
//   emitByte(arg & 0xff);
// }

static void emitByteArg(uint8_t instruction, uint8_t arg) {
  emitByte(instruction);
  emitByte(arg);
}

static void emitByteArgs(uint8_t instruction, uint8_t arg1, uint8_t arg2) {
  emitByte(instruction);
  emitByte(arg1);
  emitByte(arg2);
}

// Uncomment if needed, delete if not needed
// static void emitShortArg(uint8_t instruction, int arg) {
//   emitByte(instruction);
//   emitShort(arg);
// }

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
    emitByteArg(OP_GET_LOCAL, 0);
  } else {
    emitByte(OP_NONE);
  }

  emitByte(OP_RETURN);
}

static uint8_t makeConstant(Value value) {
  int constant = addConstant(currentChunk(), value);
  if (constant > UINT8_MAX) {
    error("Too many constants in one chunk");
    return 0;
  }

  return (uint8_t)constant;
}

static void emitConstant(Value value) {
  emitByteArg(OP_CONSTANT, makeConstant(value));
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

  compiler->function = newFunction();
  current = compiler;

  if (type == TYPE_LAMBDA) {
    current->function->name = copyStringLength("\b", 1);
  } else if (type != TYPE_SCRIPT) {
    current->function->name = copyStringLength(parser.previous.start, parser.previous.length);
  }

  Local* local = &current->locals[current->localCount++];
  local->depth = 0;
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
  emitReturn();
  ObjFunction* function = current->function;

#if DEBUG_PRINT_CODE
  if (!parser.hadError) {
    disassembleChunk(currentChunk(), function->name != NULL ? function->name->chars : "<script>");
  }
#endif

  while (current->loop != NULL) {
    if (current->loop->breaks != NULL) free(current->loop->breaks);
    current->loop = current->loop->enclosing;
  }

  current = current->enclosing;
  return function;
}

static void pushScope() { current->scopeDepth++; }

static int discardLocals(int depth) {
  int local = current->localCount - 1;
  while (local >= 0 && current->locals[local].depth >= depth) {
    // Helpful code to print out discarded locals:
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

static uint8_t identifierConstant(Token* name) {
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

static void addLocal(Token name) {
  if (current->localCount == UINT8_COUNT) {
    error("Too many local variables in function");
    return;
  }

  Local* local = &current->locals[current->localCount++];
  local->name = name;
  local->depth = -1;
  local->isCaptured = false;
}

static void declareVariable() {
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

  addLocal(*name);
}

static uint8_t parseVariable(const char* errorMessage) {
  expect(TOKEN_IDENTIFIER, errorMessage);

  declareVariable();
  if (current->scopeDepth > 0) return 0;

  return identifierConstant(&parser.previous);
}

static void markInitialized() {
  if (current->scopeDepth == 0) return;
  current->locals[current->localCount - 1].depth = current->scopeDepth;
}

static void defineVariable(uint8_t global) {
  if (current->scopeDepth > 0) {
    markInitialized();
    return;
  }

  emitByteArg(OP_DEFINE_GLOBAL, global);
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

  switch (signature->type) {
    case SIG_GETTER:
      break;
    case SIG_METHOD:
    case SIG_INITIALIZER:
      signatureParameterList(name, length, signature->arity);
      break;
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
    error("Method name too long");
    signature.length = MAX_METHOD_NAME;
  }

  return signature;
}

static void finishParameterList(Signature* signature) {
  signature->asProperty = ALLOCATE(bool, MAX_PARAMETERS);
  do {
    matchLine();
    signature->arity++;

    if (signature->arity == MAX_PARAMETERS + 1) {
      error("Too many parameters");
    }

    signature->asProperty[signature->arity - 1] = match(TOKEN_PLUS);

    uint8_t constant = parseVariable("Expecting a parameter name");
    defineVariable(constant);
  } while (match(TOKEN_COMMA));
}

static uint8_t argumentList() {
  uint8_t argCount = 0;

  matchLine();
  if (!check(TOKEN_RIGHT_PAREN)) {
    do {
      matchLine();
      expression();
      if (argCount == 255) {
        error("Can't have more than 255 arguments");
      }
      argCount++;
    } while (match(TOKEN_COMMA));

    matchLine();
  }
  expect(TOKEN_RIGHT_PAREN, "Expecting ')' after arguments");
  return argCount;
}

static void binary(bool canAssign) {
  TokenType operatorType = parser.previous.type;
  ParseRule* rule = getRule(operatorType);

  matchLine();

  expressionBp((BindingPower)(rule->bp + 1));

#if METHOD_CALL_OPERATORS
  Signature signature = { rule->name, (int)strlen(rule->name), SIG_METHOD, 1 };

  char fullSignature[MAX_METHOD_SIGNATURE];
  int length;
  signatureToString(&signature, fullSignature, &length);

  Value string = OBJ_VAL(copyStringLength(fullSignature, length));
  uint8_t constant = makeConstant(string);

  emitByteArg(OP_INVOKE, constant);
  emitByte(1);
#else
  switch (operatorType) {
    case TOKEN_STAR_STAR: emitByte(OP_EXPONENT); break;
    case TOKEN_BANG_EQ:   emitByte(OP_NOT_EQUAL); break;
    case TOKEN_EQ_EQ:     emitByte(OP_EQUAL); break;
    case TOKEN_GT:        emitByte(OP_GREATER); break;
    case TOKEN_GT_EQ:     emitByte(OP_GREATER_EQUAL); break;
    case TOKEN_LT:        emitByte(OP_LESS); break;
    case TOKEN_LT_EQ:     emitByte(OP_LESS_EQUAL); break;
    case TOKEN_PLUS:      emitByte(OP_ADD); break;
    case TOKEN_MINUS:     emitByte(OP_SUBTRACT); break;
    case TOKEN_STAR:      emitByte(OP_MULTIPLY); break;
    case TOKEN_SLASH:     emitByte(OP_DIVIDE); break;
    case TOKEN_PERCENT:   emitByte(OP_MODULO); break;
    case TOKEN_PIPE:      emitByte(OP_BIT_OR); break;
    case TOKEN_CARET:     emitByte(OP_BIT_XOR); break;
    case TOKEN_AMPERSAND: emitByte(OP_BIT_AND); break;
    case TOKEN_SHL:       emitByte(OP_SHL); break;
    case TOKEN_SHR:       emitByte(OP_SHR); break;
    case TOKEN_COLON:     emitByte(OP_RANGE_EXCL); break;
    case TOKEN_DOT_DOT:   emitByte(OP_RANGE_INCL); break;
    default: return; // Unreachable.
  }
#endif
}

void binarySignature(Signature* signature) {
  signature->type = SIG_METHOD;
  signature->arity = 1;
  signature->asProperty = NULL;

  expect(TOKEN_LEFT_PAREN, "Expecting '(' after operator name");
  uint8_t constant = parseVariable("Expecting a parameter name");
  defineVariable(constant);
  expect(TOKEN_RIGHT_PAREN, "Expecting ')' after parameter name");
}

void unarySignature(Signature* signature) {
  signature->type = SIG_GETTER;
}

void mixedSignature(Signature* signature) {
  signature->type = SIG_GETTER;

  if (match(TOKEN_LEFT_PAREN)) {
    signature->type = SIG_METHOD;
    signature->arity = 1;
    signature->asProperty = NULL;

    uint8_t constant = parseVariable("Expecting a parameter name");
    defineVariable(constant);
    expect(TOKEN_RIGHT_PAREN, "Expecting ')' after parameter name");
  }
}

void namedSignature(Signature* signature) {
  signature->type = SIG_GETTER;

  if (!match(TOKEN_LEFT_PAREN)) return;

  signature->type = SIG_METHOD;

  matchLine();
  if (match(TOKEN_RIGHT_PAREN)) return;

  finishParameterList(signature);
  expect(TOKEN_RIGHT_PAREN, "Expect ')' after parameters");
}

static void call(bool canAssign) {
  uint8_t argCount = argumentList();
  emitByteArg(OP_CALL, argCount);
}

static void callFunc(bool canAssign) {
  lambda(false);
  emitByteArg(OP_CALL, 1);
}

static void dot(bool canAssign) {
  expect(TOKEN_IDENTIFIER, "Expecting a property name after '.'");
  uint8_t name = identifierConstant(&parser.previous);
  Signature signature = signatureFromToken(SIG_GETTER);

  if (canAssign && match(TOKEN_EQ)) {
    expression();
    emitByteArg(OP_SET_PROPERTY, name);
  } else if (match(TOKEN_LEFT_PAREN)) {
    matchLine();
    uint8_t argCount = argumentList();

    signature.arity = argCount;
    signature.type = SIG_METHOD;

    char fullSignature[MAX_METHOD_SIGNATURE];
    int length;
    signatureToString(&signature, fullSignature, &length);

    emitByteArg(OP_INVOKE, makeConstant(OBJ_VAL(copyStringLength(fullSignature, length))));
    emitByte(argCount);
  } else {
    emitByteArg(OP_GET_PROPERTY, name);
  }
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
  expect(TOKEN_LEFT_PAREN, "Expecting '(' after 'if'");
  expression();
  expect(TOKEN_RIGHT_PAREN, "Expecting ')' after condition");

  int thenJump = emitJump(OP_JUMP_FALSY);
  emitByte(OP_POP);
  expression();

  int elseJump = emitJump(OP_JUMP);

  patchJump(thenJump);
  emitByte(OP_POP);

  if (match(TOKEN_ELIF)) {
    if_(false);
  } else {
    expect(TOKEN_ELSE, "If expression must have an else clause");
    expression();
  }
  patchJump(elseJump);
}

static void stringInterpolation(bool canAssign) {
  // TODO: String interpolation
  // Nothing here.
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
    matchLine();
    expression();
    emitByteArg(setOp, (uint8_t)arg);
  } else {
    emitByteArg(getOp, (uint8_t)arg);
  }
}

static void variable(bool canAssign) {
  namedVariable(parser.previous, canAssign);
}

static void list(bool canAssign) {
  emitByteArg(OP_GET_GLOBAL, makeConstant(OBJ_VAL(copyStringLength("List", 4))));
  emitByteArgs(OP_INVOKE, makeConstant(OBJ_VAL(copyStringLength("new()", 5))), 0);
  
  do {
    matchLine();

    if (check(TOKEN_RIGHT_BRACKET)) break;

    expression();
    emitByteArgs(OP_INVOKE, makeConstant(OBJ_VAL(copyStringLength("addCore(1)", 10))), 1);
  } while (match(TOKEN_COMMA));

  matchLine();
  expect(TOKEN_RIGHT_BRACKET, "Expecting ']' after list literal");
}

static void subscript(bool canAssign) {
  expressionBp(BP_NOT);
  expect(TOKEN_RIGHT_BRACKET, "Expecting ']' after index");

  if (canAssign && match(TOKEN_EQ)) {
    expression();
    emitByteArgs(OP_INVOKE, makeConstant(OBJ_VAL(copyStringLength("set(2)", 6))), 2);
  } else {
    emitByteArgs(OP_INVOKE, makeConstant(OBJ_VAL(copyStringLength("get(1)", 6))), 1);
  }
}

static Token syntheticToken(const char* text) {
  Token token;
  token.start = text;
  token.length = (int)strlen(text);
  return token;
}

static void super_(bool canAssign) {
  if (currentClass == NULL) {
    error("Can't use 'super' outside of a class");
  } else if (!currentClass->hasSuperclass) {
    error("Can't use 'super' in a class with no superclass");
  }

  expect(TOKEN_DOT, "Expecting '.' after 'super'");
  expect(TOKEN_IDENTIFIER, "Expecting a superclass method name");
  uint8_t name = identifierConstant(&parser.previous);

  namedVariable(syntheticToken("this"), false);
  if (match(TOKEN_LEFT_PAREN)) {
    matchLine();
    uint8_t argCount = argumentList();
    namedVariable(syntheticToken("super"), false);
    emitByteArg(OP_SUPER_INVOKE, name);
    emitByte(argCount);
  } else {
    namedVariable(syntheticToken("super"), false);
    emitByteArg(OP_GET_SUPER, name);
  }
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

static void unary(bool canAssign) {
  TokenType operatorType = parser.previous.type;
#if METHOD_CALL_OPERATORS
  ParseRule* rule = getRule(operatorType);
#endif

  matchLine();

  // Compile the operand.
  expressionBp(operatorType == TOKEN_NOT ? BP_NOT : BP_UNARY);

#if METHOD_CALL_OPERATORS
  emitByte(OP_INVOKE);
  emitByteArg(makeConstant(OBJ_VAL(copyString(rule->name))), 0);
#else
  switch (operatorType) {
    case TOKEN_NOT: emitByte(OP_NOT); break;
    case TOKEN_MINUS: emitByte(OP_NEGATE); break;
    default: return; // Unreachable
  }
#endif
}

#if METHOD_CALL_OPERATORS
#define UNUSED                    { NULL,   NULL,   BP_NONE, NULL, NULL }
#define INFIX(fn, bp)             { NULL,   fn,     bp,      NULL, NULL }
#define INFIX_OPERATOR(bp, name)  { NULL,   binary, bp,      name, binarySignature }
#define PREFIX(fn, bp)            { fn,     NULL,   bp,      NULL, NULL }
#define PREFIX_OPERATOR(bp, name) { unary,  NULL,   bp,      name, unarySignature }
#define BOTH(prefix, infix, bp)   { prefix, infix,  bp,      NULL, NULL }
#define OPERATOR(bp, name)        { unary,  binary, bp,      name, mixedSignature }
#else
#define UNUSED                    { NULL, NULL, BP_NONE }
#define INFIX(fn, bp)             { NULL, fn, bp }
#define INFIX_OPERATOR(bp, name)  { NULL, binary, bp }
#define PREFIX(fn, bp)            { fn, NULL, bp }
#define PREFIX_OPERATOR(bp, name) { unary, NULL, bp }
#define BOTH(prefix, infix, bp)   { prefix, infix, bp }
#define OPERATOR(bp, name)        { unary, binary, bp }
#endif

ParseRule rules[] = {
  /* TOKEN_LEFT_PAREN    */ BOTH(grouping, call, BP_CALL),
  /* TOKEN_RIGHT_PAREN   */ UNUSED,
  /* TOKEN_LEFT_BRACKET  */ BOTH(list, subscript, BP_CALL),
  /* TOKEN_RIGHT_BRACKET */ UNUSED,
  /* TOKEN_LEFT_BRACE    */ BOTH(lambda, callFunc, BP_CALL),
  /* TOKEN_RIGHT_BRACE   */ UNUSED,
  /* TOKEN_SEMICOLON     */ UNUSED,
  /* TOKEN_COMMA         */ UNUSED,
  /* TOKEN_COLON         */ INFIX_OPERATOR(BP_RANGE, ":"),
  /* TOKEN_PLUS          */ INFIX_OPERATOR(BP_TERM, "+"),
  /* TOKEN_SLASH         */ INFIX_OPERATOR(BP_FACTOR, "/"),
  /* TOKEN_PERCENT       */ INFIX_OPERATOR(BP_FACTOR, "%"),
  /* TOKEN_PIPE          */ INFIX_OPERATOR(BP_BIT_OR, "|"),
  /* TOKEN_CARET         */ INFIX_OPERATOR(BP_BIT_XOR, "^"),
  /* TOKEN_AMPERSAND     */ INFIX_OPERATOR(BP_BIT_AND, "&"),
  /* TOKEN_DOT           */ INFIX(dot, BP_CALL),
  /* TOKEN_DOT_DOT       */ INFIX_OPERATOR(BP_RANGE, ".."),
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
#if METHOD_CALL_OPERATORS
  /* TOKEN_IDENTIFIER    */ { variable, NULL, BP_NONE, NULL, namedSignature },
#else
  /* TOKEN_IDENTIFIER    */ PREFIX(variable, BP_NONE),
#endif
  /* TOKEN_STRING        */ PREFIX(literal, BP_NONE),
  /* TOKEN_INTERPOLATION */ PREFIX(stringInterpolation, BP_NONE),
  /* TOKEN_NUMBER        */ PREFIX(literal, BP_NONE),
  /* TOKEN_AND           */ INFIX(and_, BP_AND),
  /* TOKEN_BREAK         */ UNUSED,
  /* TOKEN_CLASS         */ UNUSED,
  /* TOKEN_CONTINUE      */ UNUSED,
  /* TOKEN_EACH          */ UNUSED,
  /* TOKEN_ELIF          */ UNUSED,
  /* TOKEN_ELSE          */ UNUSED,
  /* TOKEN_FALSE         */ PREFIX(literal, BP_NONE),
  /* TOKEN_FOR           */ UNUSED,
  /* TOKEN_FUN           */ UNUSED,
  /* TOKEN_IF            */ PREFIX(if_, BP_NONE),
  /* TOKEN_IN            */ UNUSED,
  /* TOKEN_IS            */ UNUSED,
  /* TOKEN_NONE          */ PREFIX(literal, BP_NONE),
  /* TOKEN_NOT           */ PREFIX_OPERATOR(BP_NOT, "not"),
  /* TOKEN_OR            */ INFIX(or_, BP_OR),
  /* TOKEN_PASS          */ UNUSED,
  /* TOKEN_PRINT         */ UNUSED,
  /* TOKEN_RETURN        */ UNUSED,
  /* TOKEN_SHL           */ INFIX_OPERATOR(BP_BIT_SHIFT, "shl"),
  /* TOKEN_SHR           */ INFIX_OPERATOR(BP_BIT_SHIFT, "shr"),
  /* TOKEN_STATIC        */ UNUSED,
  /* TOKEN_SUPER         */ PREFIX(super_, BP_NONE),
  /* TOKEN_THIS          */ PREFIX(this_, BP_NONE),
  /* TOKEN_TRUE          */ PREFIX(literal, BP_NONE),
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

static void block(bool indentationBased) {
  matchLine();
  if (!indentationBased) ignoreIndentation();

  int blockEnd;
  char* message;
  if (indentationBased) {
    blockEnd = TOKEN_DEDENT;
    message = "Expecting indentation to decrease after block";
  } else {
    blockEnd = TOKEN_RIGHT_BRACE;
    message = "Expecting '}' after block";
  }

  while (!check(blockEnd) && !check(TOKEN_EOF)) {
    if (!indentationBased) ignoreIndentation();

    if (!check(blockEnd)) {
      declaration();

      if (parser.previous.type != TOKEN_DEDENT) {
        if (!check(blockEnd) && (!indentationBased || !check(TOKEN_EOF))) {
          expectStatementEnd("Expecting a newline after statement");
        }
      }
    }
  }

  if (!indentationBased || !check(TOKEN_EOF)) expect(blockEnd, message);
}

static void function(FunctionType type) {
  Compiler compiler;
  initCompiler(&compiler, type);
  pushScope();

  expect(TOKEN_LEFT_PAREN, "Expecting '(' after function name");
  if (!check(TOKEN_RIGHT_PAREN)) {
    do {
      matchLine();
      current->function->arity++;
      if (current->function->arity > 255) {
        errorAtCurrent("Can't have more than 255 parameters");
      }

      uint8_t constant = parseVariable("Expecting a parameter name");
      defineVariable(constant);
    } while (match(TOKEN_COMMA));
  }
  expect(TOKEN_RIGHT_PAREN, "Expecting ')' after parameters");

  if (matchLine()) {
    expect(TOKEN_INDENT, "Expecting an indent before function body");
    block(true);
  } else {
    expect(TOKEN_LEFT_BRACE, "Expecting '{' before function body");
    block(false);
  }

  ObjFunction* function = endCompiler();
  emitByteArg(OP_CLOSURE, makeConstant(OBJ_VAL(function)));

  for (int i = 0; i < function->upvalueCount; i++) {
    emitByte(compiler.upvalues[i].isLocal ? 1 : 0);
    emitByte(compiler.upvalues[i].index);
  }
}

static void lambda(bool canAssign) {
  Compiler compiler;
  initCompiler(&compiler, TYPE_LAMBDA);
  pushScope();

  matchLine();

  if (match(TOKEN_PIPE)) {
    if (!match(TOKEN_PIPE)) {
      do {
        matchLine();
        current->function->arity++;
        if (current->function->arity > 255) {
          errorAtCurrent("Can't have more than 255 parameters");
        }
        uint8_t constant = parseVariable("Expecting a parameter name");
        defineVariable(constant);
      } while (match(TOKEN_COMMA));
      expect(TOKEN_PIPE, "Expecting '|' after parameters");
      matchLine();
    }
  }

  block(false);

  ObjFunction* function = endCompiler();
  emitByteArg(OP_CLOSURE, makeConstant(OBJ_VAL(function)));

  for (int i = 0; i < function->upvalueCount; i++) {
    emitByte(compiler.upvalues[i].isLocal ? 1 : 0);
    emitByte(compiler.upvalues[i].index);
  }
}

static void method() {
  bool isStatic = match(TOKEN_STATIC);

#if METHOD_CALL_OPERATORS

  SignatureFn signatureFn = getRule(parser.current.type)->signatureFn;
  advance();

  if (signatureFn == NULL) {
    error("Expecting a method definition");
    return;
  }

  Signature signature = signatureFromToken(SIG_GETTER);

  FunctionType type = isStatic ? TYPE_STATIC_METHOD : TYPE_METHOD;
  if (parser.previous.length == 4 && memcmp(parser.previous.start, "init", 4) == 0) {
    if (isStatic) {
      error("Initializers cannot be static");
    }
    signature.type = SIG_INITIALIZER;
    type = TYPE_INITIALIZER;
  }

  signatureFn(&signature);

  char fullSignature[MAX_METHOD_SIGNATURE];
  int length;
  signatureToString(&signature, fullSignature, &length);

  uint8_t constant = makeConstant(OBJ_VAL(copyStringLength(fullSignature, length)));

#else

  expect(TOKEN_IDENTIFIER, "Expecting a method name");
  uint8_t constant = identifierConstant(&parser.previous);

  FunctionType type = isStatic ? TYPE_STATIC_METHOD : TYPE_METHOD;
  if (parser.previous.length == 4 && memcmp(parser.previous.start, "init", 4) == 0) {
    if (isStatic) {
      error("Initializers cannot be static");
    }
    type = TYPE_INITIALIZER;
  }

#endif

  Compiler compiler;
  initCompiler(&compiler, type);
  pushScope();

  if (signature.asProperty != NULL) {
    if (isStatic) error("Can only store fields through non-static methods");
    for (int i = 1; i <= current->function->arity; i++) {
      if (signature.asProperty[i - 1]) {
        emitByteArg(OP_GET_LOCAL, 0);
        emitByteArg(OP_GET_LOCAL, i);
        Local local = current->locals[i];
        emitByteArg(OP_SET_PROPERTY, identifierConstant(&local.name));
        emitByte(OP_POP);
      }
    }
    FREE_ARRAY(bool, signature.asProperty, MAX_PARAMETERS);
  }

  if (matchLine()) {
    expect(TOKEN_INDENT, "Expecting an indent before method body");
    block(true);
  } else {
    expect(TOKEN_LEFT_BRACE, "Expecting '{' before method body");
    block(false);
  }

  ObjFunction* result = endCompiler();
  emitByteArg(OP_CLOSURE, makeConstant(OBJ_VAL(result)));

  for (int i = 0; i < result->upvalueCount; i++) {
    emitByte(compiler.upvalues[i].isLocal ? 1 : 0);
    emitByte(compiler.upvalues[i].index);
  }

  emitByte(OP_METHOD_INSTANCE + isStatic);
  emitByte(constant);
}

static void classDeclaration() {
  expect(TOKEN_IDENTIFIER, "Expecting a class name");
  Token className = parser.previous;
  uint8_t nameConstant = identifierConstant(&parser.previous);
  declareVariable();

  emitByteArg(OP_CLASS, nameConstant);
  defineVariable(nameConstant);

  ClassCompiler classCompiler;
  classCompiler.hasSuperclass = false;
  classCompiler.enclosing = currentClass;
  currentClass = &classCompiler;

  if (match(TOKEN_LT)) {
    expect(TOKEN_IDENTIFIER, "Expecting a superclass name");
    variable(false);

    if (identifiersEqual(&className, &parser.previous)) {
      error("A class can't inherit from itself");
    }

    pushScope();
    addLocal(syntheticToken("super"));
    defineVariable(0);

    classCompiler.hasSuperclass = true;
  } else {
    emitByteArg(OP_GET_GLOBAL, makeConstant(OBJ_VAL(copyString("Object"))));
  }

  namedVariable(className, false);
  emitByte(OP_INHERIT);
  emitByte(OP_POP); // Superclass

  namedVariable(className, false);

  bool indentationBased;
  int blockEnd;
  char* message;
  if (matchLine()) {
    indentationBased = true;
    blockEnd = TOKEN_DEDENT;
    message = "Expecting indentation to decrease after class body";

    expect(TOKEN_INDENT, "Expecting an indent before class body");
  } else {
    indentationBased = false;
    blockEnd = TOKEN_RIGHT_BRACE;
    message = "Expecting '}' after class body";

    expect(TOKEN_LEFT_BRACE, "Expecting '{' before class body");
  }

  matchLine();

  while (!check(blockEnd) && !check(TOKEN_EOF)) {
    if (!indentationBased) ignoreIndentation();
    method();
    matchLine();
  }

  if (!indentationBased || !check(TOKEN_EOF)) expect(blockEnd, message);
  emitByte(OP_POP);

  if (classCompiler.hasSuperclass) {
    popScope();
  }

  currentClass = currentClass->enclosing;
}

static void funDeclaration() {
  uint8_t global = parseVariable("Expecting a function name");
  markInitialized();
  function(TYPE_FUNCTION);
  defineVariable(global);
}

static void varDeclaration() {
  uint8_t global = parseVariable("Expecting a variable name");

  if (match(TOKEN_EQ)) {
    matchLine();
    expression();
  } else {
    emitByte(OP_NONE);
  }

  defineVariable(global);
}

static void expressionStatement() {
  expression();
  emitByte(OP_POP);
}

static void forStatement() {
  pushScope();

  Token* label = NULL;
  if (match(TOKEN_COLON)) {
    expect(TOKEN_IDENTIFIER, "Expecting a loop label");
    label = ALLOCATE(Token, 1);
    memcpy(label, &parser.previous, sizeof(Token));
  }

  expect(TOKEN_LEFT_PAREN, "Expecting '(' after 'for'");
  matchLine();

  if (match(TOKEN_SEMICOLON)) {
    // No initializer.
  } else if (match(TOKEN_VAR)) {
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

  if (!match(TOKEN_RIGHT_PAREN)) {
    int bodyJump = emitJump(OP_JUMP);
    int incrementStart = currentChunk()->count;
    expressionStatement();

    matchLine();
    expect(TOKEN_RIGHT_PAREN, "Expecting ')' after for clauses");

    emitLoop(current->loop->start);
    current->loop->start = incrementStart;
    patchJump(bodyJump);
  }

  statement();

  endLoop();
  popScope();

  FREE(Token, label);
}

static void eachStatement() {
  pushScope(); // Scope for hidden iterator variables
  expect(TOKEN_LEFT_PAREN, "Expecting '(' after 'each'");
  expect(TOKEN_IDENTIFIER, "Expecting a loop variable");
  Token name = parser.previous;

  expect(TOKEN_IN, "Expecting 'in' after loop variable");
  matchLine();

  expression();

  if (current->localCount + 2 > UINT8_COUNT) {
    error("Cannot declare any more locals.");
    return;
  }

  addLocal(syntheticToken("`seq"));
  markInitialized();
  int seqSlot = current->localCount - 1;
  emitByte(OP_NONE);
  addLocal(syntheticToken("`iter"));
  markInitialized();
  int iterSlot = current->localCount - 1;

  matchLine();
  expect(TOKEN_RIGHT_PAREN, "Expecting ')' after loop expression");

  Loop loop;
  startLoop(&loop);

  emitByteArg(OP_GET_LOCAL, seqSlot);
  emitByteArg(OP_GET_LOCAL, iterSlot);

  emitByteArgs(OP_INVOKE, makeConstant(OBJ_VAL(copyStringLength("iterate(1)", 10))), 1);
  emitByteArg(OP_SET_LOCAL, iterSlot);

  current->loop->exitJump = emitJump(OP_JUMP_FALSY);

  emitByteArg(OP_GET_LOCAL, seqSlot);
  emitByteArg(OP_GET_LOCAL, iterSlot);
  emitByteArgs(OP_INVOKE, makeConstant(OBJ_VAL(copyStringLength("iteratorValue(1)", 16))), 1);

  pushScope(); // Loop variable
  addLocal(name);
  markInitialized();

  statement();

  popScope(); // Loop variable

  endLoop();

  popScope(); // `seq and `iter variables
}

static void ifStatement() {
  expect(TOKEN_LEFT_PAREN, "Expecting '(' after 'if'");
  matchLine();

  expression();

  matchLine();
  expect(TOKEN_RIGHT_PAREN, "Expecting ')' after condition");

  int thenJump = emitJump(OP_JUMP_FALSY);
  emitByte(OP_POP);
  if (matchLine()) {
    expect(TOKEN_INDENT, "Expecting an indent before body");
    block(true);
  }
  else statement();

  int elseJump = emitJump(OP_JUMP);

  patchJump(thenJump);
  emitByte(OP_POP);

  if (match(TOKEN_ELIF)) ifStatement();
  else if (match(TOKEN_ELSE)) {
    if (matchLine()) {
      expect(TOKEN_INDENT, "Expecting an indent before body");
      block(true);
    }
    else statement();
  }
  patchJump(elseJump);
}

static void printStatement() {
  expression();
  emitByte(OP_PRINT);
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
        break;
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
        break;
      }
    }
  }

  discardLocals(loop->scopeDepth + 1);

  // Jump to the top of the loop.
  emitLoop(loop->start);

  FREE(Token, label);
}

/*
static void returnStatement() {
  Token* label = NULL;
  if (match(TOKEN_COLON)) {
    expect(TOKEN_IDENTIFIER, "Expecting a function name after ':'");
    label = ALLOCATE(Token, 1);
    memcpy(label, &parser.previous, sizeof(Token));
  }

  // The vm finds one return and ignores the later ones.
  // The other returns will need to be added to the enclosing function.
  int depth = 1;
  Compiler* compiler = current;

  if (label != NULL) {
    for (;; depth++) {
      if (compiler->function->name != NULL && label->length == compiler->function->name->length) {
        if (memcmp(compiler->function->name->chars, label->start, label->length) == 0) break;
      }

      compiler = compiler->enclosing;
      if (compiler == NULL) {
        error("Can't find function with this name");
        depth = 0;
        break;
      }
    }
  }

  if (check(TOKEN_LINE) || check(TOKEN_SEMICOLON)) {
    for (int i = 0; i < depth; i++) emitReturn();
  } else {
    for (int i = 0; i < depth; i++) {
      if (i + 1 == depth) {
        if (current->type == TYPE_INITIALIZER) {
          error("Can't return a value from an initializer");
        }

        expression();
        emitByte(OP_RETURN);
      } else emitReturn();
    }
  }

  FREE(Token, label);
}
*/

static void returnStatement() {
  if (check(TOKEN_LINE) || check(TOKEN_SEMICOLON)) {
    emitReturn();
  } else {
    if (current->type == TYPE_INITIALIZER) {
      error("Can't return a value from an initializer");
    }

    expression();
    emitByte(OP_RETURN);
  }
}

#define MAX_WHEN_CASES 256

static void whenStatement() {
  expect(TOKEN_LEFT_PAREN, "Expecting '(' after 'when'");
  expression();
  // TODO: Possibly make custom operators, like this:
  //
  // when (variable)
  //   >= 3 -> something()
  //   == 3 -> somethingElse()
  //   3 -> somethingElse() # same as ==
  //
  expect(TOKEN_RIGHT_PAREN, "Expecting ')' after value");

  bool indentationBased;
  int blockEnd;
  char* message;
  if (matchLine()) {
    indentationBased = true;
    blockEnd = TOKEN_DEDENT;
    message = "Expecting indentation to decrease after cases";

    expect(TOKEN_INDENT, "Expecting an indent before when statement cases");
  } else {
    indentationBased = false;
    blockEnd = TOKEN_RIGHT_BRACE;
    message = "Expecting '}' after when statement body";

    expect(TOKEN_LEFT_BRACE, "Expecting '{' before when statement cases");
  }

  matchLine();

  int state = 0; // 0 at very start, 1 before default, 2 after default
  int caseEnds[MAX_WHEN_CASES];
  int caseCount = 0;
  int previousCaseSkip = -1;

  if (check(blockEnd)) errorAtCurrent("When statement must have at least one case");

  while (!check(blockEnd) && !check(TOKEN_EOF)) {
    if (!indentationBased) ignoreIndentation();

    if (match(TOKEN_IS) || match(TOKEN_ELSE)) {
      TokenType caseType = parser.previous.type;

      if (state == 2) {
        error("Can't have any cases after the default case");
      }

      if (state == 1) {
        if (caseCount == MAX_WHEN_CASES) {
          error("Too many cases");
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
        // when (var)
        //   is 3 | 4 -> print "3 or 4"

        expect(TOKEN_RIGHT_ARROW, "Expecting '->' after case value");

#if METHOD_CALL_OPERATORS
        emitByteArgs(OP_INVOKE, makeConstant(OBJ_VAL(copyStringLength("==", 2))), 1);
#else
        emitByte(OP_EQUAL);
#endif
        previousCaseSkip = emitJump(OP_JUMP_FALSY);

        // Pop the comparison result
        emitByte(OP_POP);

        if (matchLine()) {
          expect(TOKEN_INDENT, "Expecting an indent before block");
          block(true);
        }
      } else {
        if (state == 0) {
          error("Can't have a default case first");
        }
        state = 2;
        expect(TOKEN_RIGHT_ARROW, "Expecting '->' after else case");
        previousCaseSkip = -1;

        if (matchLine()) {
          expect(TOKEN_INDENT, "Expecting an indent before block");
          block(true);
        }
      }
    } else {
      if (state == 0) {
        error("Can't have statements before any case");
      }
      statement();
      if (!indentationBased || !check(TOKEN_EOF)) expectStatementEnd("Expecting a newline after statement");
    }
  }

  if (!indentationBased || !check(TOKEN_EOF)) expect(blockEnd, message);

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

  expect(TOKEN_LEFT_PAREN, "Expecting '(' after 'while'");
  matchLine();

  expression();

  matchLine();
  expect(TOKEN_RIGHT_PAREN, "Expecting ')' after condition");

  current->loop->exitJump = emitJump(OP_JUMP_FALSY);
  emitByte(OP_POP);
  statement();

  endLoop();

  FREE(Token, label);
}

static void synchronize() {
  parser.panicMode = false;

  while (parser.current.type != TOKEN_EOF) {
    if (parser.previous.type == TOKEN_SEMICOLON) return;
    switch (parser.current.type) {
      case TOKEN_CLASS:
      case TOKEN_FUN:
      case TOKEN_VAR:
      case TOKEN_FOR:
      case TOKEN_EACH:
      case TOKEN_IF:
      case TOKEN_WHILE:
      case TOKEN_WHEN:
      case TOKEN_PRINT:
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
  else if (match(TOKEN_VAR)) varDeclaration();
  else statement();

  if (parser.panicMode) synchronize();
}

static void statement() {
  if (match(TOKEN_PRINT)) printStatement();
  else if (match(TOKEN_PASS)) return;
  else if (match(TOKEN_BREAK)) breakStatement();
  else if (match(TOKEN_CONTINUE)) continueStatement();
  else if (match(TOKEN_FOR)) forStatement();
  else if (match(TOKEN_EACH)) eachStatement();
  else if (match(TOKEN_IF)) ifStatement();
  else if (match(TOKEN_WHEN)) whenStatement();
  else if (match(TOKEN_RETURN)) returnStatement();
  else if (match(TOKEN_WHILE)) whileStatement();
  else if (match(TOKEN_LEFT_BRACE)) {
    pushScope();
    block(false);
    popScope();
  } else expressionStatement();
}

ObjFunction* compile(const char* source, const char* module) {
  initLexer(source);
  Compiler compiler;
  initCompiler(&compiler, TYPE_SCRIPT);

  parser.module = module;
  parser.hadError = false;
  parser.panicMode = false;

  advance();

#if DEBUG_PRINT_TOKENS
  do {
    printf("%d\n", parser.current.type);
    advance();
  } while (!match(TOKEN_EOF));

  return NULL;
#endif

  matchLine();
  if (match(TOKEN_INDENT)) error("Unexpected indentation");

  while (!match(TOKEN_EOF)) {
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

  ObjFunction* function = endCompiler();
  return parser.hadError ? NULL : function;
}

void markCompilerRoots() {
  Compiler* compiler = current;
  while (compiler != NULL) {
    markObject((Obj*)compiler->function);
    compiler = compiler->enclosing;
  }
}
