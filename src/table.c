#include "table.h"

#include <stdlib.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "type.h"
#include "value.h"
#include "vm.h"

#define TABLE_MAX_LOAD 0.75

void initTable(Table* table) {
  table->count = 0;
  table->capacity = 0;
  table->entries = NULL;
}

void freeTable(Table* table) {
  FREE_ARRAY(Entry, table->entries, table->capacity);
  initTable(table);
}

void freeTypeTable(TypeTable* table) {
  for (int i = 0; i < table->capacity; i++) {
    if (table->entries[i].type == NULL) continue;

    Type* type = table->entries[i].type;
    FREE_ARRAY(Type, type->supertypes, type->supertypeCapacity);
    clearSupertypes(type);
    FREE(Type, type);
  }

  FREE_ARRAY(TypePtr, table->entries, table->capacity);
  initTable((Table*)table);
}

static Entry* findEntry(Entry* entries, int capacity, ObjString* key) {
  uint32_t index = key->hash & (capacity - 1);
  Entry* tombstone = NULL;

  for (;;) {
    Entry* entry = &entries[index];
    if (entry->key == NULL) {
      if (IS_NONE(entry->value)) {
        // Empty entry.
        return tombstone != NULL ? tombstone : entry;
      } else {
        // We found a tombstone.
        if (tombstone == NULL) tombstone = entry;
      }
    } else if (entry->key == key) {
      // We found the key.
      return entry;
    }

    index = (index + 1) & (capacity - 1);
  }
}

static TypePtr* findType(TypePtr* entries, int capacity, ObjString* key) {
  uint32_t index = key->hash & (capacity - 1);
  TypePtr* tombstone;
  bool foundTombstone = false;

  for (;;) {
    TypePtr* entry = entries + index;
    if (entry->type == NULL) {
      if (entry->isTombstone == false) {
        // Empty entry.
        return foundTombstone ? tombstone : entry;
      } else {
        // Tombstone.
        if (tombstone->type != NULL) {
          tombstone = entry;
          foundTombstone = true;
        }
      }
    } else if (entry->type->name == key) {
      // Found it!
      return entry;
    }

    index = (index + 1) & (capacity - 1);
  }
}

bool tableGet(Table* table, ObjString* key, Value* value) {
  ASSERT(table != NULL, "Table cannot be NULL");

  if (table->count == 0) return false;

  Entry* entry = findEntry(table->entries, table->capacity, key);
  if (entry->key == NULL) return false;

  *value = entry->value;
  return true;
}

Type* typeTableGet(TypeTable* table, ObjString* key) {
  ASSERT(table != NULL, "TypeTable cannot be NULL");

  if (table->count == 0) return NULL;

  TypePtr* entry = findType(table->entries, table->capacity, key);
  if (entry->type == NULL) return NULL;

  return entry->type;
}

bool tableContains(Table* table, ObjString* key) {
  ASSERT(table != NULL, "Table cannot be NULL");

  if (table->count == 0) return false;

  Entry* entry = findEntry(table->entries, table->capacity, key);
  return entry->key != NULL;
}

static void adjustCapacity(Table* table, int capacity) {
  ASSERT(table != NULL, "Table cannot be NULL");

  Entry* entries = ALLOCATE(Entry, capacity);
  for (int i = 0; i < capacity; i++) {
    entries[i].key = NULL;
    entries[i].value = NONE_VAL;
    entries[i].isMutable = true;
  }

  table->count = 0;
  for (int i = 0; i < table->capacity; i++) {
    Entry* entry = &table->entries[i];
    if (entry->key == NULL) continue;

    Entry* dest = findEntry(entries, capacity, entry->key);
    dest->key = entry->key;
    dest->value = entry->value;
    dest->isMutable = entry->isMutable;
    table->count++;
  }

  FREE_ARRAY(Entry, table->entries, table->capacity);
  table->entries = entries;
  table->capacity = capacity;
}

static void adjustTypeTableCapacity(TypeTable* table, int capacity) {
  TypePtr* entries = ALLOCATE(TypePtr, capacity);
  for (int i = 0; i < capacity; i++) {
    entries[i].type = NULL;
    entries[i].isTombstone = false;
  }

  table->count = 0;
  for (int i = 0; i < table->capacity; i++) {
    Type* entry = table->entries[i].type;
    if (entry == NULL) continue;

    TypePtr* dest = findType(entries, capacity, entry->name);
    dest->type = entry;
    table->count++;
  }

  FREE_ARRAY(TypePtr, table->entries, table->capacity);
  table->entries = entries;
  table->capacity = capacity;
}

bool tableSet(Table* table, ObjString* key, Value value, bool isMutable) {
  ASSERT(table != NULL, "Table cannot be NULL");

  if (IS_OBJ(value)) pushRoot(AS_OBJ(value));

  if (table->count + 1 > table->capacity * TABLE_MAX_LOAD) {
    int capacity = GROW_CAPACITY(table->capacity);
    adjustCapacity(table, capacity);
  }

  if (IS_OBJ(value)) popRoot();

  Entry* entry = findEntry(table->entries, table->capacity, key);
  bool isNewKey = entry->key == NULL;
  if (isNewKey && IS_NONE(entry->value)) table->count++;

  entry->key = key;
  entry->value = value;
  entry->isMutable = isMutable;
  return isNewKey;
}

void typeTableAdd(TypeTable* table, Type* type) {
  ASSERT(table != NULL, "Table cannot be NULL");

  if (table->count + 1 > table->capacity * TABLE_MAX_LOAD) {
    int capacity = GROW_CAPACITY(table->capacity);
    adjustTypeTableCapacity(table, capacity);
  }

  ASSERT(table->entries != NULL, "Table entries can't be NULL");

  TypePtr* entry = findType(table->entries, table->capacity, type->name);
  if (entry->type == NULL && entry->isTombstone == false) table->count++;

  entry->type = type;
  entry->isTombstone = false;
}

bool tableSetMutable(Table* table, ObjString* key, Value value, bool isMutable) {
  ASSERT(table != NULL, "Table cannot be NULL");

  if (table->count + 1 > table->capacity * TABLE_MAX_LOAD) {
    int capacity = GROW_CAPACITY(table->capacity);
    adjustCapacity(table, capacity);
  }

  Entry* entry = findEntry(table->entries, table->capacity, key);
  bool isNewKey = entry->key == NULL;

  if (!entry->isMutable) return false;

  if (isNewKey && IS_NONE(entry->value)) table->count++;

  entry->key = key;
  entry->value = value;
  entry->isMutable = isMutable;
  return true;
}

bool tableDelete(Table* table, ObjString* key) {
  ASSERT(table != NULL, "Table cannot be NULL");

  if (table->count == 0) return false;

  // Find the entry.
  Entry* entry = findEntry(table->entries, table->capacity, key);
  if (entry->key == NULL) return false;

  // Place a tombstone in the entry.
  entry->key = NULL;
  entry->value = UNDEFINED_VAL;
  return true;
}

void tableAddAll(Table* from, Table* to, bool isMutable) {
  ASSERT(from != NULL, "Source table cannot be NULL");
  ASSERT(to != NULL, "Destination table cannot be NULL");

  for (int i = 0; i < from->capacity; i++) {
    Entry* entry = &from->entries[i];
    if (entry->key != NULL) {
      tableSet(to, entry->key, entry->value, isMutable);
    }
  }
}

ObjString* tableFindString(Table* table, const char* chars, int length, uint32_t hash) {
  ASSERT(table != NULL, "Table cannot be NULL");

  if (table->count == 0) return NULL;

  uint32_t index = hash & (table->capacity - 1);
  for (;;) {
    Entry* entry = &table->entries[index];
    if (entry->key == NULL) {
      // Stop if we find an empty non-tombstone entry.
      if (IS_NONE(entry->value)) return NULL;
    } else if (entry->key->length == length && entry->key->hash == hash && memcmp(entry->key->chars, chars, length) == 0) {
      // We found it.
      return entry->key;
    }

    index = (index + 1) & (table->capacity - 1);
  }
}

void tableRemoveWhite(Table* table) {
  ASSERT(table != NULL, "Table cannot be NULL");

  for (int i = 0; i < table->capacity; i++) {
    Entry* entry = &table->entries[i];
    if (entry->key != NULL && !entry->key->obj.isMarked) {
      tableDelete(table, entry->key);
    }
  }
}

void markTable(Table* table) {
  ASSERT(table != NULL, "Table cannot be NULL");

  for (int i = 0; i < table->capacity; i++) {
    Entry* entry = &table->entries[i];
    markObject((Obj*)entry->key);
    markValue(entry->value);
  }
}
