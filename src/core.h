#pragma once

#include "vm.h"
#include "compiler.h"

#if STATIC_TYPING()
void initializeCoreTypes(TypeTable* types, TypeChecker* typeChecker);
#endif
void initializeCore(VM* vm);
