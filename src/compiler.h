#ifndef flicker_compiler_h
#define flicker_compiler_h

#include "object.h"
#include "vm.h"

ObjFunction* compile(const char* source, ObjModule* module, bool inRepl);
void markCompilerRoots();

#endif
