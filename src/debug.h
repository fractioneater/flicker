#ifndef flicker_debug_h
#define flicker_debug_h

#include "chunk.h"
#include "vm.h"

void printStack(VM* vm);
void disassembleChunk(Chunk* chunk, const char* name);
int disassembleInstruction(Chunk* chunk, int offset);

#endif
