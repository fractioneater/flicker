#pragma once

#include "common.h"
#include "type.h"
#include "value.h"

typedef struct Type Type;

#define DECLARE_BASIC_TABLE(name, type) \
  typedef struct {                      \
    int count;                          \
    int capacity;                       \
    type* entries;                      \
  } name;                               \
                                        \
  void init##name(name* table);         \
  void free##name(name* table);

#define DEFINE_BASIC_TABLE(name, type)                 \
  void init##name(name* table) {                       \
    ASSERT(table != NULL, "Table cannot be NULL");     \
                                                       \
    table->count = 0;                                  \
    table->capacity = 0;                               \
    table->entries = NULL;                             \
  }                                                    \
                                                       \
  void free##name(name* table) {                       \
    ASSERT(table != NULL, "Table cannot be NULL");     \
                                                       \
    FREE_ARRAY(type, table->entries, table->capacity); \
    init##name(table);                                 \
  }

typedef struct {
  ObjString* key;
  Value value;
  bool isMutable;
} Entry;

DECLARE_BASIC_TABLE(Table, Entry)
DECLARE_BASIC_TABLE(TypeTable, Type)

bool tableGet(Table* table, ObjString* key, Value* value);
Type* typeTableGet(TypeTable* table, ObjString* key);
bool tableContains(Table* table, ObjString* key);
bool tableSet(Table* table, ObjString* key, Value value, bool isMutable);
bool tableSetMutable(Table* table, ObjString* key, Value value, bool isMutable);
bool typeTableAdd(TypeTable* table, Type* type);
bool tableDelete(Table* table, ObjString* key);
void tableAddAll(Table* from, Table* to, bool isMutable);
ObjString* tableFindString(Table* table, const char* chars, int length, uint32_t hash);

void tableRemoveWhite(Table* table);
void markTable(Table* table);
