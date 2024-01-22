#pragma once

#include "vm.h"

#if STATIC_TYPING()
void initializeCoreTypes(TypeTable* types);
#endif
void initializeCore(VM* vm);
