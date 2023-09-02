#include "debug.h"

#include <stdio.h>

#include "object.h"
#include "value.h"

void disassembleChunk(Chunk* chunk, const char* name) {
  printf("== %s ==\n", name);
  for (int offset = 0; offset < chunk->count;) {
    offset = disassembleInstruction(chunk, offset);
  }
}

static int constantInstruction(const char* name, Chunk* chunk, int offset) {
  uint8_t constant = chunk->code[offset + 1];
  printf("%-16s %4d '", name, constant);
  printValue(chunk->constants.values[constant]);
  printf("'\n");
  return offset + 2;
}

static int invokeInstruction(const char* name, Chunk* chunk, int offset) {
  uint8_t constant = chunk->code[offset + 1];
  uint8_t argCount = chunk->code[offset] - OP_INVOKE_0;
  printf("%-16s %4d '", name, constant);
  printValue(chunk->constants.values[constant]);
  printf("'  (%d args)\n", argCount);
  return offset + 2;
}

static int simpleInstruction(const char* name, int offset) {
  printf("%s\n", name);
  return offset + 1;
}

static int byteInstruction(const char* name, Chunk* chunk, int offset) {
  uint8_t slot = chunk->code[offset + 1];
  printf("%-16s %4d\n", name, slot);
  return offset + 2;
}

static int jumpInstruction(const char* name, int sign, Chunk* chunk, int offset) {
  uint16_t jump = (uint16_t)(chunk->code[offset + 1] << 8);
  jump |= chunk->code[offset + 2];
  printf("%-16s %4d -> %d\n", name, offset, offset + 3 + sign * jump);
  return offset + 3;
}

int disassembleInstruction(Chunk* chunk, int offset) {
  printf("%04d ", offset);
  if (offset > 0 && chunk->lines[offset] == chunk->lines[offset - 1]) {
    printf("   | ");
  } else {
    printf("%4d ", chunk->lines[offset]);
  }

  uint8_t instruction = chunk->code[offset];
  switch (instruction) {
    case OP_CONSTANT:
      return constantInstruction("OP_CONSTANT", chunk, offset);
    case OP_NONE:
      return simpleInstruction("OP_NONE", offset);
    case OP_TRUE:
      return simpleInstruction("OP_TRUE", offset);
    case OP_FALSE:
      return simpleInstruction("OP_FALSE", offset);
    case OP_POP:
      return simpleInstruction("OP_POP", offset);
    case OP_DUP:
      return simpleInstruction("OP_DUP", offset);
    case OP_GET_LOCAL:
      return byteInstruction("OP_GET_LOCAL", chunk, offset);
    case OP_SET_LOCAL:
      return byteInstruction("OP_SET_LOCAL", chunk, offset);
    case OP_GET_GLOBAL:
      return constantInstruction("OP_GET_GLOBAL", chunk, offset);
    case OP_DEFINE_GLOBAL:
      return constantInstruction("OP_DEFINE_GLOBAL", chunk, offset);
    case OP_SET_GLOBAL:
      return constantInstruction("OP_SET_GLOBAL", chunk, offset);
    case OP_GET_UPVALUE:
      return byteInstruction("OP_GET_UPVALUE", chunk, offset);
    case OP_SET_UPVALUE:
      return byteInstruction("OP_SET_UPVALUE", chunk, offset);
    case OP_GET_METHOD:
      return constantInstruction("OP_GET_METHOD", chunk, offset);
    case OP_GET_PROPERTY:
      return constantInstruction("OP_GET_PROPERTY", chunk, offset);
    case OP_SET_PROPERTY:
      return constantInstruction("OP_SET_PROPERTY", chunk, offset);
    case OP_GET_SUPER:
      return constantInstruction("OP_GET_SUPER", chunk, offset);
#if !METHOD_CALL_OPERATORS
    case OP_EQUAL:
      return simpleInstruction("OP_EQUAL", offset);
    case OP_NOT_EQUAL:
      return simpleInstruction("OP_NOT_EQUAL", offset);
    case OP_GREATER:
      return simpleInstruction("OP_GREATER", offset);
    case OP_GREATER_EQUAL:
      return simpleInstruction("OP_GREATER_EQUAL", offset);
    case OP_LESS:
      return simpleInstruction("OP_LESS", offset);
    case OP_LESS_EQUAL:
      return simpleInstruction("OP_LESS_EQUAL", offset);
    case OP_BIT_OR:
      return simpleInstruction("OP_BIT_OR", offset);
    case OP_BIT_XOR:
      return simpleInstruction("OP_BIT_XOR", offset);
    case OP_BIT_AND:
      return simpleInstruction("OP_BIT_AND", offset);
    case OP_SHL:
      return simpleInstruction("OP_SHL", offset);
    case OP_SHR:
      return simpleInstruction("OP_SHR", offset);
    case OP_RANGE_EXCL:
      return simpleInstruction("OP_RANGE_EXCL", offset);
    case OP_RANGE_INCL:
      return simpleInstruction("OP_RANGE_INCL", offset);
    case OP_ADD:
      return simpleInstruction("OP_ADD", offset);
    case OP_SUBTRACT:
      return simpleInstruction("OP_SUBTRACT", offset);
    case OP_MULTIPLY:
      return simpleInstruction("OP_MULTIPLY", offset);
    case OP_DIVIDE:
      return simpleInstruction("OP_DIVIDE", offset);
    case OP_MODULO:
      return simpleInstruction("OP_MODULO", offset);
    case OP_EXPONENT:
      return simpleInstruction("OP_EXPONENT", offset);
    case OP_NOT:
      return simpleInstruction("OP_NOT", offset);
    case OP_NEGATE:
      return simpleInstruction("OP_NEGATE", offset);
#endif
    case OP_PRINT:
      return simpleInstruction("OP_PRINT", offset);
    case OP_JUMP:
      return jumpInstruction("OP_JUMP", 1, chunk, offset);
    case OP_JUMP_FALSY:
      return jumpInstruction("OP_JUMP_FALSY", 1, chunk, offset);
    case OP_JUMP_TRUTHY:
      return jumpInstruction("OP_JUMP_TRUTHY", 1, chunk, offset);
    case OP_LOOP:
      return jumpInstruction("OP_LOOP", -1, chunk, offset);
    // Just so you know, I didn't type this next section by hand.
  case OP_CALL_0:
    return simpleInstruction("OP_CALL_0", offset);
  case OP_CALL_1:
    return simpleInstruction("OP_CALL_1", offset);
  case OP_CALL_2:
    return simpleInstruction("OP_CALL_2", offset);
  case OP_CALL_3:
    return simpleInstruction("OP_CALL_3", offset);
  case OP_CALL_4:
    return simpleInstruction("OP_CALL_4", offset);
  case OP_CALL_5:
    return simpleInstruction("OP_CALL_5", offset);
  case OP_CALL_6:
    return simpleInstruction("OP_CALL_6", offset);
  case OP_CALL_7:
    return simpleInstruction("OP_CALL_7", offset);
  case OP_CALL_8:
    return simpleInstruction("OP_CALL_8", offset);
  case OP_CALL_9:
    return simpleInstruction("OP_CALL_9", offset);
  case OP_CALL_10:
    return simpleInstruction("OP_CALL_10", offset);
  case OP_CALL_11:
    return simpleInstruction("OP_CALL_11", offset);
  case OP_CALL_12:
    return simpleInstruction("OP_CALL_12", offset);
  case OP_CALL_13:
    return simpleInstruction("OP_CALL_13", offset);
  case OP_CALL_14:
    return simpleInstruction("OP_CALL_14", offset);
  case OP_CALL_15:
    return simpleInstruction("OP_CALL_15", offset);
  case OP_CALL_16:
    return simpleInstruction("OP_CALL_16", offset);
  case OP_INVOKE_0:
    return invokeInstruction("OP_INVOKE_0", chunk, offset);
  case OP_INVOKE_1:
    return invokeInstruction("OP_INVOKE_1", chunk, offset);
  case OP_INVOKE_2:
    return invokeInstruction("OP_INVOKE_2", chunk, offset);
  case OP_INVOKE_3:
    return invokeInstruction("OP_INVOKE_3", chunk, offset);
  case OP_INVOKE_4:
    return invokeInstruction("OP_INVOKE_4", chunk, offset);
  case OP_INVOKE_5:
    return invokeInstruction("OP_INVOKE_5", chunk, offset);
  case OP_INVOKE_6:
    return invokeInstruction("OP_INVOKE_6", chunk, offset);
  case OP_INVOKE_7:
    return invokeInstruction("OP_INVOKE_7", chunk, offset);
  case OP_INVOKE_8:
    return invokeInstruction("OP_INVOKE_8", chunk, offset);
  case OP_INVOKE_9:
    return invokeInstruction("OP_INVOKE_9", chunk, offset);
  case OP_INVOKE_10:
    return invokeInstruction("OP_INVOKE_10", chunk, offset);
  case OP_INVOKE_11:
    return invokeInstruction("OP_INVOKE_11", chunk, offset);
  case OP_INVOKE_12:
    return invokeInstruction("OP_INVOKE_12", chunk, offset);
  case OP_INVOKE_13:
    return invokeInstruction("OP_INVOKE_13", chunk, offset);
  case OP_INVOKE_14:
    return invokeInstruction("OP_INVOKE_14", chunk, offset);
  case OP_INVOKE_15:
    return invokeInstruction("OP_INVOKE_15", chunk, offset);
  case OP_INVOKE_16:
    return invokeInstruction("OP_INVOKE_16", chunk, offset);
  case OP_SUPER_0:
    return invokeInstruction("OP_SUPER_0", chunk, offset);
  case OP_SUPER_1:
    return invokeInstruction("OP_SUPER_1", chunk, offset);
  case OP_SUPER_2:
    return invokeInstruction("OP_SUPER_2", chunk, offset);
  case OP_SUPER_3:
    return invokeInstruction("OP_SUPER_3", chunk, offset);
  case OP_SUPER_4:
    return invokeInstruction("OP_SUPER_4", chunk, offset);
  case OP_SUPER_5:
    return invokeInstruction("OP_SUPER_5", chunk, offset);
  case OP_SUPER_6:
    return invokeInstruction("OP_SUPER_6", chunk, offset);
  case OP_SUPER_7:
    return invokeInstruction("OP_SUPER_7", chunk, offset);
  case OP_SUPER_8:
    return invokeInstruction("OP_SUPER_8", chunk, offset);
  case OP_SUPER_9:
    return invokeInstruction("OP_SUPER_9", chunk, offset);
  case OP_SUPER_10:
    return invokeInstruction("OP_SUPER_10", chunk, offset);
  case OP_SUPER_11:
    return invokeInstruction("OP_SUPER_11", chunk, offset);
  case OP_SUPER_12:
    return invokeInstruction("OP_SUPER_12", chunk, offset);
  case OP_SUPER_13:
    return invokeInstruction("OP_SUPER_13", chunk, offset);
  case OP_SUPER_14:
    return invokeInstruction("OP_SUPER_14", chunk, offset);
  case OP_SUPER_15:
    return invokeInstruction("OP_SUPER_15", chunk, offset);
  case OP_SUPER_16:
    return invokeInstruction("OP_SUPER_16", chunk, offset);
    case OP_CLOSURE: {
      offset++;
      uint8_t constant = chunk->code[offset++];
      printf("%-16s %4d ", "OP_CLOSURE", constant);
      printValue(chunk->constants.values[constant]);
      printf("\n");

      ObjFunction* function = AS_FUNCTION(chunk->constants.values[constant]);
      for (int j = 0; j < function->upvalueCount; j++) {
        int isLocal = chunk->code[offset++];
        int index = chunk->code[offset++];
        printf("%04d    |                             %s %d\n", offset - 2,
               isLocal ? "local" : "upvalue", index);
      }

      return offset;
    }
    case OP_CLOSE_UPVALUE:
      return simpleInstruction("OP_CLOSE_UPVALUE", offset);
    case OP_RETURN:
      return simpleInstruction("OP_RETURN", offset);
    case OP_CLASS:
      return constantInstruction("OP_CLASS", chunk, offset);
    case OP_METHOD_INSTANCE:
      return constantInstruction("OP_METHOD_INSTANCE", chunk, offset);
    case OP_METHOD_STATIC:
      return constantInstruction("OP_METHOD_STATIC", chunk, offset);
    default:
      printf("Unknown opcode %d\n", instruction);
      return offset + 1;
  }
}
