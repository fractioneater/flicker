#pragma once

#include "common.h"
#include "value.h"

#if STATIC_TYPING()

typedef struct Type Type;

typedef struct {
  bool isTombstone;
  Type* type;
} TypePtr;

typedef struct {
  int count;
  int capacity;
  TypePtr* entries;
} TypeTable;

typedef struct Method Method;

typedef struct {
  int count;
  int capacity;
  Method* entries;
} MethodTable;

#endif

typedef struct {
  ObjString* key;
  Value value;
  bool isMutable;
} Entry;

typedef struct {
  int count;
  int capacity;
  Entry* entries;
} Table;

void initTable(Table* table);
void freeTable(Table* table);

#if STATIC_TYPING()
void freeTypeTable(TypeTable* table);
Type* typeTableGet(TypeTable* table, ObjString* key);
void typeTableAdd(TypeTable* table, Type* type);

void freeMethodTable(MethodTable* table);
bool methodTableGet(MethodTable* table, ObjString* name, Method* out);
void methodTableAdd(MethodTable* table, Method method);
#endif

bool tableGet(Table* table, ObjString* key, Value* value);
bool tableContains(Table* table, ObjString* key);
bool tableSet(Table* table, ObjString* key, Value value, bool isMutable);
bool tableSetMutable(Table* table, ObjString* key, Value value, bool isMutable);
bool tableDelete(Table* table, ObjString* key);
void tableAddAll(Table* from, Table* to, bool isMutable);
ObjString* tableFindString(Table* table, const char* chars, int length, uint32_t hash);

void tableRemoveWhite(Table* table);
void markTable(Table* table);
