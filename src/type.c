#include "type.h"

#include "memory.h"

void clearSupertypes(Type* type) {
  reallocate(type->supertypes, 0, 0);

  type->supertypes = NULL;
  type->supertypeCount = 0;
  type->supertypeCapacity = 0;
}

void addSupertype(Type* type, Type* supertype) {
  if (type->supertypeCapacity < type->supertypeCount + 1) {
    int oldCapacity = type->supertypeCapacity;
    type->supertypeCapacity = GROW_CAPACITY(oldCapacity);
    type->supertypes = GROW_ARRAY(Type, type->supertypes, oldCapacity, type->supertypeCapacity);
  }
  
  type->supertypes[type->supertypeCount++] = *supertype;
}

void freeSupertypes(Type* type) {
  FREE_ARRAY(Type, type->supertypes, type->supertypeCapacity);
  clearSupertypes(type);
}
