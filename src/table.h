#ifndef flicker_table_h
#define flicker_table_h

#include "common.h"
#include "value.h"

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
bool tableGet(Table* table, ObjString* key, Value* value);
bool tableContains(Table* table, ObjString* key);
bool tableSet(Table* table, ObjString* key, Value value, bool isMutable);
bool tableSetMutable(Table* table, ObjString* key, Value value, bool isMutable);
bool tableDelete(Table* table, ObjString* key);
void tableAddAll(Table* from, Table* to, bool isMutable);
ObjString* tableFindString(Table* table, const char* chars, int length, uint32_t hash);

void tableRemoveWhite(Table* table);
void markTable(Table* table);

#endif
