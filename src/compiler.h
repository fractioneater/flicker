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

#if STATIC_TYPING()
typedef struct {
  // A hash table of all the types the compiler uses.
  TypeTable types;

  // The type of the current expression.
  Type* expressionType;

  // Types that the compiler should be able to access easily.
  Type* objectOptionalType;
  Type* unitType;
  Type* boolType;
  Type* functionType;
  Type* numberType;
  Type* stringType;
  Type* listType;
  Type* mapType;
  Type* nothingOptionalType;
} TypeChecker;
#endif

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
