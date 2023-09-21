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
  OP_BIND_METHOD,
  OP_BIND_SUPER,
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
  OP_ERROR,
  OP_JUMP,
  OP_JUMP_FALSY,
  OP_JUMP_TRUTHY,
  OP_LOOP,

  OP_CALL_0,
  OP_CALL_1,
  OP_CALL_2,
  OP_CALL_3,
  OP_CALL_4,
  OP_CALL_5,
  OP_CALL_6,
  OP_CALL_7,
  OP_CALL_8,
  OP_CALL_9,
  OP_CALL_10,
  OP_CALL_11,
  OP_CALL_12,
  OP_CALL_13,
  OP_CALL_14,
  OP_CALL_15,
  OP_CALL_16,

  OP_INVOKE_0,
  OP_INVOKE_1,
  OP_INVOKE_2,
  OP_INVOKE_3,
  OP_INVOKE_4,
  OP_INVOKE_5,
  OP_INVOKE_6,
  OP_INVOKE_7,
  OP_INVOKE_8,
  OP_INVOKE_9,
  OP_INVOKE_10,
  OP_INVOKE_11,
  OP_INVOKE_12,
  OP_INVOKE_13,
  OP_INVOKE_14,
  OP_INVOKE_15,
  OP_INVOKE_16,

  OP_SUPER_0,
  OP_SUPER_1,
  OP_SUPER_2,
  OP_SUPER_3,
  OP_SUPER_4,
  OP_SUPER_5,
  OP_SUPER_6,
  OP_SUPER_7,
  OP_SUPER_8,
  OP_SUPER_9,
  OP_SUPER_10,
  OP_SUPER_11,
  OP_SUPER_12,
  OP_SUPER_13,
  OP_SUPER_14,
  OP_SUPER_15,
  OP_SUPER_16,

  OP_CLOSURE,
  OP_CLOSE_UPVALUE,
  OP_RETURN,
  OP_CLASS,
  OP_INITIALIZER,
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
