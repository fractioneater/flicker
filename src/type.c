#include "type.h"

#if STATIC_TYPING()

#include "compiler.h"
#include "memory.h"
#include "table.h"

Type* newType(TypeTable* table, ObjString* name) {
  Type* type = (Type*)reallocate(NULL, 0, sizeof(Type));
  type->name = name;
  type->methods = (MethodTable*)reallocate(NULL, 0, sizeof(MethodTable));
  initTable((Table*)type->methods);
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
  if (type == match) return true;
  for (int i = 0; i < type->supertypeCount; i++) {
    // TODO: Recursion is probably not the best way to do this.
    if (hasSupertype(type->supertypes[i], match)) return true;
  }
  return false;
}

static bool parameterListsMatch(int arity, Parameter* expected, Parameter* actual) {
  for (int p = 0; p < arity; p++) {
    if (actual[p].type == expected[p].type) continue;
    if (!hasSupertype(actual[p].type, expected[p].type)) return false;
  }
  return true;
}

// TODO NEXT: Get this new type of signatures working in the compiler (methods only for now).

// TODO NEXT NEXT: Fix the uninitialized value errors (valgrind flicker)
bool getSignature(SignatureList* methods, int arity, Parameter* parameters, Signature* out) {
  for (int s = 0; s < methods->signatureCount; s++) {
    Signature signature = methods->signatures[s];
    if (signature.arity != arity) continue;
    if (!parameterListsMatch(arity, signature.parameters, parameters)) continue;
    *out = signature;
    return true;
  }
  return false;
}

#endif
