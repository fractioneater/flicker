#include "type.h"

#include "memory.h"

Type* newType(TypeTable* table, ObjString* name) {
  Type* type = (Type*)reallocate(NULL, 0, sizeof(Type));
  type->name = name;
  type->methods = (MethodList){};
  clearSupertypes(type);
  typeTableAdd(table, type);
  return type;
}

void freeType(Type* type) {
  FREE_ARRAY(char, type->name->chars, type->name->length + 1);
  freeSupertypes(type);
  FREE(Type, type);
}

void clearSupertypes(Type* type) {
  type->supertypes = NULL;
  type->supertypeCount = 0;
  type->supertypeCapacity = 0;
}

void freeSupertypes(Type* type) {
  FREE_ARRAY(Type, type->supertypes, type->supertypeCapacity);
  clearSupertypes(type);
}

void addSupertype(Type* type, Type* supertype) {
  if (type->supertypeCapacity < type->supertypeCount + 1) {
    int oldCapacity = type->supertypeCapacity;
    type->supertypeCapacity = GROW_CAPACITY(oldCapacity);
    type->supertypes = GROW_ARRAY(Type*, type->supertypes, oldCapacity, type->supertypeCapacity);
  }

  ASSERT(type->supertypes != NULL, "Supertypes cannot be NULL");
  
  type->supertypes[type->supertypeCount++] = supertype;
}

bool hasSupertype(Type* type, Type* match) {
  for (int i = 0; i < type->supertypeCount; i++) {
    if (type->supertypes[i]->name == match->name) return true;
  }
  return false;
}
