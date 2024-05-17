#include "debug.h"

#include <stdio.h>
#include <stdlib.h>

#include "object.h"
#include "value.h"

void printStack(VM* vm) {
  for (Value* slot = vm->stack; slot < vm->stackTop; slot++) {
    printf("[ ");
    printValue(*slot);
    printf(" ]");
  }
  printf("\n");
}

void disassembleChunk(Chunk* chunk, const char* name) {
  printf("== %s ==\n", name);
  for (int offset = 0; offset < chunk->count;) {
    offset = disassembleInstruction(chunk, offset);
  }
}

static int variableConstant(Chunk* chunk, int offset) {
  uint8_t constant = chunk->code[offset + 1];
  if (constant >= 0x80) {
    uint8_t second = chunk->code[offset + 2];
    return (((int)constant & 0x7f) << 8) | second;
  }
  return constant;
}

static int constantInstruction(const char* name, Chunk* chunk, int offset) {
  int constant = variableConstant(chunk, offset);
  printf("%-16s %4d '", name, constant);
  printValue(chunk->constants.values[constant]);
  printf("'\n");
  return offset + (constant >= 0x80 ? 3 : 2);
}

static int invokeInstruction(const char* name, Chunk* chunk, int offset) {
  int constant = variableConstant(chunk, offset);
  uint8_t argCount;
  if (name[3] == 'S') {
    argCount = chunk->code[offset] - OP_SUPER_0;
  } else {
    argCount = chunk->code[offset] - OP_INVOKE_0;
  }
  printf("%-16s %4d '", name, constant);
  printValue(chunk->constants.values[constant]);
  printf("'  (%d args)\n", argCount);
  return offset + (constant >= 0x80 ? 3 : 2);
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
      return constantInstruction("CONSTANT", chunk, offset);
    case OP_NONE:
      return simpleInstruction("NONE", offset);
    case OP_TRUE:
      return simpleInstruction("TRUE", offset);
    case OP_FALSE:
      return simpleInstruction("FALSE", offset);
    case OP_POP:
      return simpleInstruction("POP", offset);
    case OP_DUP:
      return byteInstruction("DUP", chunk, offset);
    case OP_GET_LOCAL:
      return byteInstruction("GET_LOCAL", chunk, offset);
    case OP_SET_LOCAL:
      return byteInstruction("SET_LOCAL", chunk, offset);
    case OP_GET_GLOBAL:
      return constantInstruction("GET_GLOBAL", chunk, offset);
    case OP_DEFINE_GLOBAL:
      return constantInstruction("DEFINE_GLOBAL", chunk, offset);
    case OP_DEFINE_IMMUTABLE_GLOBAL:
      return constantInstruction("DEFINE_IMMUTABLE_GLOBAL", chunk, offset);
    case OP_SET_GLOBAL:
      return constantInstruction("SET_GLOBAL", chunk, offset);
    case OP_GET_UPVALUE:
      return byteInstruction("GET_UPVALUE", chunk, offset);
    case OP_SET_UPVALUE:
      return byteInstruction("SET_UPVALUE", chunk, offset);
    case OP_GET_PROPERTY:
      return constantInstruction("GET_PROPERTY", chunk, offset);
    case OP_SET_PROPERTY:
      return constantInstruction("SET_PROPERTY", chunk, offset);
    case OP_BIND_METHOD:
      return constantInstruction("BIND_METHOD", chunk, offset);
    case OP_BIND_SUPER:
      return constantInstruction("BIND_SUPER", chunk, offset);
    case OP_PRINT:
      return simpleInstruction("PRINT", offset);
    case OP_ERROR:
      return simpleInstruction("ERROR", offset);
    case OP_JUMP:
      return jumpInstruction("JUMP", 1, chunk, offset);
    case OP_JUMP_FALSY:
      return jumpInstruction("JUMP_FALSY", 1, chunk, offset);
    case OP_JUMP_TRUTHY:
      return jumpInstruction("JUMP_TRUTHY", 1, chunk, offset);
    case OP_JUMP_TRUTHY_POP:
      return jumpInstruction("JUMP_TRUTHY_POP", 1, chunk, offset);
    case OP_LOOP:
      return jumpInstruction("LOOP", -1, chunk, offset);
    // Just so you know, I didn't type this next section by hand.
    case OP_CALL_0:
      return simpleInstruction("CALL_0", offset);
    case OP_CALL_1:
      return simpleInstruction("CALL_1", offset);
    case OP_CALL_2:
      return simpleInstruction("CALL_2", offset);
    case OP_CALL_3:
      return simpleInstruction("CALL_3", offset);
    case OP_CALL_4:
      return simpleInstruction("CALL_4", offset);
    case OP_CALL_5:
      return simpleInstruction("CALL_5", offset);
    case OP_CALL_6:
      return simpleInstruction("CALL_6", offset);
    case OP_CALL_7:
      return simpleInstruction("CALL_7", offset);
    case OP_CALL_8:
      return simpleInstruction("CALL_8", offset);
    case OP_CALL_9:
      return simpleInstruction("CALL_9", offset);
    case OP_CALL_10:
      return simpleInstruction("CALL_10", offset);
    case OP_CALL_11:
      return simpleInstruction("CALL_11", offset);
    case OP_CALL_12:
      return simpleInstruction("CALL_12", offset);
    case OP_CALL_13:
      return simpleInstruction("CALL_13", offset);
    case OP_CALL_14:
      return simpleInstruction("CALL_14", offset);
    case OP_CALL_15:
      return simpleInstruction("CALL_15", offset);
    case OP_CALL_16:
      return simpleInstruction("CALL_16", offset);
    case OP_INVOKE_0:
      return invokeInstruction("INVOKE_0", chunk, offset);
    case OP_INVOKE_1:
      return invokeInstruction("INVOKE_1", chunk, offset);
    case OP_INVOKE_2:
      return invokeInstruction("INVOKE_2", chunk, offset);
    case OP_INVOKE_3:
      return invokeInstruction("INVOKE_3", chunk, offset);
    case OP_INVOKE_4:
      return invokeInstruction("INVOKE_4", chunk, offset);
    case OP_INVOKE_5:
      return invokeInstruction("INVOKE_5", chunk, offset);
    case OP_INVOKE_6:
      return invokeInstruction("INVOKE_6", chunk, offset);
    case OP_INVOKE_7:
      return invokeInstruction("INVOKE_7", chunk, offset);
    case OP_INVOKE_8:
      return invokeInstruction("INVOKE_8", chunk, offset);
    case OP_INVOKE_9:
      return invokeInstruction("INVOKE_9", chunk, offset);
    case OP_INVOKE_10:
      return invokeInstruction("INVOKE_10", chunk, offset);
    case OP_INVOKE_11:
      return invokeInstruction("INVOKE_11", chunk, offset);
    case OP_INVOKE_12:
      return invokeInstruction("INVOKE_12", chunk, offset);
    case OP_INVOKE_13:
      return invokeInstruction("INVOKE_13", chunk, offset);
    case OP_INVOKE_14:
      return invokeInstruction("INVOKE_14", chunk, offset);
    case OP_INVOKE_15:
      return invokeInstruction("INVOKE_15", chunk, offset);
    case OP_INVOKE_16:
      return invokeInstruction("INVOKE_16", chunk, offset);
    case OP_SUPER_0:
      return invokeInstruction("SUPER_0", chunk, offset);
    case OP_SUPER_1:
      return invokeInstruction("SUPER_1", chunk, offset);
    case OP_SUPER_2:
      return invokeInstruction("SUPER_2", chunk, offset);
    case OP_SUPER_3:
      return invokeInstruction("SUPER_3", chunk, offset);
    case OP_SUPER_4:
      return invokeInstruction("SUPER_4", chunk, offset);
    case OP_SUPER_5:
      return invokeInstruction("SUPER_5", chunk, offset);
    case OP_SUPER_6:
      return invokeInstruction("SUPER_6", chunk, offset);
    case OP_SUPER_7:
      return invokeInstruction("SUPER_7", chunk, offset);
    case OP_SUPER_8:
      return invokeInstruction("SUPER_8", chunk, offset);
    case OP_SUPER_9:
      return invokeInstruction("SUPER_9", chunk, offset);
    case OP_SUPER_10:
      return invokeInstruction("SUPER_10", chunk, offset);
    case OP_SUPER_11:
      return invokeInstruction("SUPER_11", chunk, offset);
    case OP_SUPER_12:
      return invokeInstruction("SUPER_12", chunk, offset);
    case OP_SUPER_13:
      return invokeInstruction("SUPER_13", chunk, offset);
    case OP_SUPER_14:
      return invokeInstruction("SUPER_14", chunk, offset);
    case OP_SUPER_15:
      return invokeInstruction("SUPER_15", chunk, offset);
    case OP_SUPER_16:
      return invokeInstruction("SUPER_16", chunk, offset);
    case OP_IMPORT_MODULE:
      return constantInstruction("IMPORT_MODULE", chunk, offset);
    case OP_IMPORT_VARIABLE:
      return constantInstruction("IMPORT_VARIABLE", chunk, offset);
    case OP_IMPORT_ALL_VARIABLES:
      return simpleInstruction("IMPORT_ALL_VARIABLES", offset);
    case OP_END_MODULE:
      return simpleInstruction("END_MODULE", offset);
    case OP_TUPLE:
      return byteInstruction("TUPLE", chunk, offset);
    case OP_CLOSURE: {
      int constant = variableConstant(chunk, offset);
      offset += constant >= 0x80 ? 3 : 2;
      printf("%-16s %4d ", "CLOSURE", constant);
      printValue(chunk->constants.values[constant]);
      printf("\n");

      ObjFunction* function = AS_FUNCTION(chunk->constants.values[constant]);
      for (int j = 0; j < function->upvalueCount; j++) {
        int isLocal = chunk->code[offset++];
        int index = chunk->code[offset++];
        printf("%04d    |   (closure var)       %s %d\n", offset - 2,
               isLocal ? "local" : "upvalue", index);
      }

      return offset;
    }
    case OP_CLOSE_UPVALUE:
      return simpleInstruction("CLOSE_UPVALUE", offset);
    case OP_RETURN:
      return simpleInstruction("RETURN", offset);
    case OP_RETURN_OUTPUT:
      return simpleInstruction("RETURN_OUTPUT", offset);
    case OP_CLASS:
      return constantInstruction("CLASS", chunk, offset);
    case OP_METHOD_INSTANCE:
      return constantInstruction("METHOD_INSTANCE", chunk, offset);
    case OP_METHOD_STATIC:
      return constantInstruction("METHOD_STATIC", chunk, offset);
    default:
      printf("Unknown opcode %d\n", instruction);
      return offset + 1;
  }
}
