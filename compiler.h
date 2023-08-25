#ifndef flicker_compiler_h
#define flicker_compiler_h

#include "object.h"
#include "vm.h"

ObjFunction* compile(const char* source, const char* module);
void markCompilerRoots();

#endif
