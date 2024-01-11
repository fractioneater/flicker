#pragma once

#include "chunk.h"
#include "vm.h"

void printStack(VM* vm, Value* stackTop);
void disassembleChunk(Chunk* chunk, const char* name);
int disassembleInstruction(Chunk* chunk, int offset);
