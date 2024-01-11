#pragma once

#include "object.h"
#include "vm.h"

ObjFunction* compile(const char* source, ObjModule* module, bool inRepl);
void markCompilerRoots();
