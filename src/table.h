#pragma once

#include "common.h"
#include "value.h"

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
void freeTypeTable(TypeTable* table);

bool tableGet(Table* table, ObjString* key, Value* value);
Type* typeTableGet(TypeTable* table, ObjString* key);
bool tableContains(Table* table, ObjString* key);
bool tableSet(Table* table, ObjString* key, Value value, bool isMutable);
bool tableSetMutable(Table* table, ObjString* key, Value value, bool isMutable);
void typeTableAdd(TypeTable* table, Type* type);
bool tableDelete(Table* table, ObjString* key);
void tableAddAll(Table* from, Table* to, bool isMutable);
ObjString* tableFindString(Table* table, const char* chars, int length, uint32_t hash);

void tableRemoveWhite(Table* table);
void markTable(Table* table);
