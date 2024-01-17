#pragma once

#include <stdlib.h>

#include "object.h"

typedef struct {

} MethodList;

typedef struct Type Type;

struct Type {
  ObjString* name;
  MethodList methods;
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
