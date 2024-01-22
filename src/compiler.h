#pragma once

#include "object.h"
#include "vm.h"

#if STATIC_TYPING()

struct Parameter {
  Type* type;
  bool storeProperty;
};

struct Signature {
  const char* name;
  int length;
  int arity;
  struct Parameter* parameters;
  Type* returnType;
};

typedef struct Parameter Parameter;
typedef struct Signature Signature;

#else

typedef struct {
  const char* name;
  int length;
  int arity;
  bool* asProperty;
} Signature;

#endif

ObjFunction* compile(const char* source, ObjModule* module, bool inRepl);
void markCompilerRoots();
