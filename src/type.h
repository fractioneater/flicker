#pragma once

#include <stdlib.h>

#include "common.h"
#if STATIC_TYPING()

#include "object.h"

typedef struct Signature Signature;
typedef struct Parameter Parameter;

typedef struct {
  int signatureCount;
  int signatureCapacity;
  Signature* signatures;
} SignatureList;

typedef struct Method Method;

struct Method {
  bool isTombstone;
  bool isSingle;
  ObjString* name;
  union {
    SignatureList* list;
    Signature* one;
  } as;
};

typedef struct Type Type;

struct Type {
  ObjString* name;
  MethodTable* methods;
  int supertypeCount;
  int supertypeCapacity;
  Type** supertypes;
};

Type* newType(TypeTable* table, ObjString* name);
void freeType(Type* type);
void clearSupertypes(Type* type);
void freeSupertypes(Type* type);
void addSupertype(Type* type, Type* supertype);
bool hasSupertype(Type* type, Type* match);

bool getSignature(SignatureList* methods, int arity, Parameter* parameters, Signature* out);

#endif
