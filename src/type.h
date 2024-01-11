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
  Type* supertypes;
  bool tombstone;
};

void clearSupertypes(Type* type);
void addSupertype(Type* type, Type* supertype);
void freeSupertypes(Type* type);
