#ifndef flicker_chunk_h
#define flicker_chunk_h

#include "common.h"
#include "value.h"

typedef enum {
  OP_CONSTANT,
  OP_NONE,
  OP_TRUE,
  OP_FALSE,
  OP_POP,
  OP_DUP,
  OP_GET_LOCAL,
  OP_SET_LOCAL,
  OP_GET_GLOBAL,
  OP_DEFINE_GLOBAL,
  OP_SET_GLOBAL,
  OP_GET_UPVALUE,
  OP_SET_UPVALUE,
  OP_GET_PROPERTY,
  OP_SET_PROPERTY,
  OP_GET_SUBSCRIPT,
  OP_SET_SUBSCRIPT,
  OP_GET_SUPER,
#if !METHOD_CALL_OPERATORS
  OP_EQUAL,
  OP_NOT_EQUAL,
  OP_GREATER,
  OP_GREATER_EQUAL,
  OP_LESS,
  OP_LESS_EQUAL,
  OP_BIT_OR,
  OP_BIT_XOR,
  OP_BIT_AND,
  OP_SHL,
  OP_SHR,
  OP_RANGE_EXCL,
  OP_RANGE_INCL,
  OP_ADD,
  OP_SUBTRACT,
  OP_MULTIPLY,
  OP_DIVIDE,
  OP_MODULO,
  OP_EXPONENT,
  OP_NOT,
  OP_NEGATE,
#endif
  OP_PRINT,
  OP_JUMP,
  OP_JUMP_FALSY,
  OP_JUMP_TRUTHY,
  OP_LOOP,
  OP_CALL,
  OP_INVOKE,
  OP_SUPER_INVOKE,
  OP_CLOSURE,
  OP_CLOSE_UPVALUE,
  OP_BUILD_LIST,
  OP_RETURN,
  OP_CLASS,
  OP_INHERIT,
  OP_METHOD_INSTANCE,
  OP_METHOD_STATIC
} OpCode;

typedef struct {
  int count;
  int capacity;
  uint8_t* code;
  int* lines;
  ValueArray constants;
} Chunk;

void initChunk(Chunk* chunk);
void freeChunk(Chunk* chunk);
void writeChunk(Chunk* chunk, uint8_t byte, int line);
int addConstant(Chunk* chunk, Value value);

#endif
