#include "object.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "memory.h"
#include "shishua.h"
#include "table.h"
#include "utils.h"
#include "value.h"
#include "vm.h"

#define ALLOCATE_OBJ(type, objectType, cls) \
  (type*)allocateObject(sizeof(type), objectType, cls)

static Obj* allocateObject(size_t size, ObjType type, ObjClass* cls) {
  Obj* obj = (Obj*)reallocate(NULL, 0, size);
  obj->type = type;
  obj->isMarked = false;
  obj->cls = cls;

  obj->next = vm.objects;
  vm.objects = obj;

# if DEBUG_LOG_GC
  printf("%p allocate %zu for %d\n", (void*)obj, size, type);
# endif

  return obj;
}

ObjBoundMethod* newBoundMethod(Value receiver, ObjClosure* method) {
  ObjBoundMethod* bound = ALLOCATE_OBJ(ObjBoundMethod, OBJ_BOUND_METHOD, vm.boundMethodClass);
  bound->receiver = receiver;
  bound->as.closure = method;
  bound->isNative = false;
  return bound;
}

ObjBoundMethod* newBoundNative(Value receiver, ObjNative* method) {
  ObjBoundMethod* bound = ALLOCATE_OBJ(ObjBoundMethod, OBJ_BOUND_METHOD, vm.boundMethodClass);
  bound->receiver = receiver;
  bound->as.native = method;
  bound->isNative = true;
  return bound;
}

ObjClass* newSingleClass(ObjString* name) {
  ObjClass* cls = ALLOCATE_OBJ(ObjClass, OBJ_CLASS, NULL);
  cls->name = name;
  cls->superclass = NULL;
  cls->initializer = UNDEFINED_VAL;
  cls->arity = 0;
  initTable(&cls->methods);
  return cls;
}

ObjClass* newClass(ObjString* name) {
  ObjString* metaclassName = stringFormat("# metaclass", name);
  pushRoot((Obj*)metaclassName);

  ObjClass* metaclass = newSingleClass(metaclassName);
  metaclass->obj.cls = vm.classClass;

  popRoot();
  pushRoot((Obj*)metaclass);

  bindSuperclass(metaclass, vm.classClass);

  ObjClass* cls = newSingleClass(name);
  pushRoot((Obj*)cls);

  cls->obj.cls = metaclass;

  popRoot();
  popRoot();

  return cls;
}

void bindSuperclass(ObjClass* subclass, ObjClass* superclass) {
  ASSERT(superclass != NULL, "Must have superclass");
  subclass->superclass = superclass;
  tableAddAll(&superclass->methods, &subclass->methods, true);
}

ObjClosure* newClosure(ObjFunction* function) {
  ObjUpvalue** upvalues = ALLOCATE(ObjUpvalue*, function->upvalueCount);
  for (int i = 0; i < function->upvalueCount; i++) {
    upvalues[i] = NULL;
  }

  ObjClosure* closure = ALLOCATE_OBJ(ObjClosure, OBJ_CLOSURE, vm.functionClass);
  closure->function = function;
  closure->upvalues = upvalues;
  closure->upvalueCount = function->upvalueCount;
  return closure;
}

ObjFunction* newFunction(ObjModule* module) {
  ObjFunction* function = ALLOCATE_OBJ(ObjFunction, OBJ_FUNCTION, vm.functionClass);
  function->arity = 0;
  function->upvalueCount = 0;
  function->name = NULL;
  function->module = module;
  initChunk(&function->chunk);
  return function;
}

ObjInstance* newInstance(ObjClass* cls) {
  ObjInstance* instance = ALLOCATE_OBJ(ObjInstance, OBJ_INSTANCE, cls);
  initTable(&instance->fields);
  return instance;
}

ObjList* newList(uint32_t count) {
  Value* array = NULL;
  if (count > 0) array = ALLOCATE(Value, count);

  ObjList* list = ALLOCATE_OBJ(ObjList, OBJ_LIST, vm.listClass);
  list->items = array;
  list->count = count;
  list->capacity = count;
  return list;
}

void listClear(ObjList* list) {
  FREE_ARRAY(Value, list->items, list->count);
  list->items = NULL;
  list->count = 0;
  list->capacity = 0;
}

void listAppend(ObjList* list, Value value) {
  if (list->capacity < list->count + 1) {
    int oldCapacity = list->capacity;
    list->capacity = GROW_CAPACITY(oldCapacity);
    list->items = GROW_ARRAY(Value, list->items, oldCapacity, list->capacity);
  }
  list->items[list->count] = value;
  list->count++;
}

void listInsertAt(ObjList* list, uint32_t index, Value value) {
  listAppend(list, NONE_VAL);

  for (uint32_t i = list->count - 1; i > index; i--) {
    list->items[i] = list->items[i - 1];
  }

  list->items[index] = value;
}

Value listDeleteAt(ObjList* list, uint32_t index) {
  Value deleted = list->items[index];
  for (uint32_t i = index; i < list->count - 1; i++) {
    list->items[i] = list->items[i + 1];
  }
  list->items[list->count - 1] = NONE_VAL;
  list->count--;

  return deleted;
}

int listIndexOf(ObjList* list, Value value) {
  for (int i = 0; i < list->count; i++) {
    if (valuesEqual(list->items[i], value)) {
      return i;
    }
  }

  return -1;
}

ObjMap* newMap() {
  ObjMap* map = ALLOCATE_OBJ(ObjMap, OBJ_MAP, vm.mapClass);
  Table table;
  initTable(&table);
  map->table = table;
  map->count = 0;
  return map;
}

Value mapGet(ObjMap* map, Value key) {
  Value value;
  if (tableGet(&map->table, AS_STRING(key), &value)) {
    return value;
  }

  return UNDEFINED_VAL;
}

void mapSet(ObjMap* map, Value key, Value value) {
  if (tableSet(&map->table, AS_STRING(key), value, false)) {
    map->count++;
  }
}

void mapClear(ObjMap* map) {
  freeTable(&map->table);
  map->count = 0;
}

void mapRemoveKey(ObjMap* map, Value key) {
  if (tableDelete(&map->table, AS_STRING(key))) {
    map->count--;
  }
}

ObjModule* newModule(ObjString* name, bool isCore) {
  ObjModule* module = ALLOCATE_OBJ(ObjModule, OBJ_MODULE, NULL);
  pushRoot((Obj*)module);
  
  initTable(&module->variables);
  module->name = name;
  module->isCore = isCore;
  popRoot();
  return module;
}

ObjNative* newNative(NativeFn function, int arity) {
  ObjNative* native = ALLOCATE_OBJ(ObjNative, OBJ_NATIVE, NULL);
  native->function = function;
  native->arity = arity;
  return native;
}

ObjPrng* newPrng(uint64_t seed[4]) {
  ObjPrng* prng = ALLOCATE_OBJ(ObjPrng, OBJ_PRNG, vm.randomClass);
  prngInit(&prng->state, seed);
  fillPrngBuffer(prng);
  return prng;
}

void fillPrngBuffer(ObjPrng* prng) {
  prngGen(&prng->state, prng->buffer, PRNG_BUFFER_SIZE);
  prng->bufferIndex = 0;
}

ObjRange* newRange(double from, double to, bool isInclusive) {
  ObjRange* range = ALLOCATE_OBJ(ObjRange, OBJ_RANGE, vm.rangeClass);
  range->from = from;
  range->to = to;
  range->isInclusive = isInclusive;
  return range;
}

static ObjString* allocateString(char* chars, int length, uint32_t hash) {
  ObjString* string = ALLOCATE_OBJ(ObjString, OBJ_STRING, vm.stringClass);
  string->length = length;
  string->chars = chars;
  string->hash = hash;

  push(OBJ_VAL(string));
  tableSet(&vm.strings, string, NONE_VAL, true);
  pop();

  return string;
}

static uint32_t hashString(const char* key, int length) {
  uint32_t hash = 2166136261u;
  for (int i = 0; i < length; i++) {
    hash ^= (uint8_t)key[i];
    hash *= 16777619;
  }
  return hash;
}

ObjString* takeString(char* chars, int length) {
  uint32_t hash = hashString(chars, length);
  ObjString* interned = tableFindString(&vm.strings, chars, length, hash);

  if (interned != NULL) {
    FREE_ARRAY(char, chars, length + 1);
    return interned;
  }

  return allocateString(chars, length, hash);
}

ObjString* copyStringLength(const char* chars, int length) {
  ASSERT(length == 0 || chars != NULL, "String should not be NULL");

  uint32_t hash = hashString(chars, length);
  ObjString* interned = tableFindString(&vm.strings, chars, length, hash);
  if (interned != NULL) return interned;

  char* heapChars = ALLOCATE(char, length + 1);
  memcpy(heapChars, chars, length);
  heapChars[length] = '\0';
  return allocateString(heapChars, length, hash);
}

ObjString* copyString(const char* chars) {
  ASSERT(chars != NULL, "String should not be NULL");
  return copyStringLength(chars, (int)strlen(chars));
}

const char* numberToCString(double value) {
  if (isnan(value)) return "NaN";
  if (isinf(value)) {
    if (value > 0.0) {
      return "Infinity";
    } else {
      return "-Infinity";
    }
  }

  char* buffer = ALLOCATE(char, 24);
  int length = sprintf(buffer, "%.14g", value);
  buffer[length] = '\0';
  return buffer;
}

ObjString* numberToString(double value) {
  if (isnan(value)) return copyStringLength("NaN", 3);
  if (isinf(value)) {
    if (value > 0.0) {
      return copyStringLength("Infinity", 8);
    } else {
      return copyStringLength("-Infinity", 9);
    }
  }

  char buffer[24];
  int length = sprintf(buffer, "%.14g", value);
  return copyStringLength(buffer, length);
}

ObjString* stringFromCodePoint(int value) {
  int length = utf8EncodeNumBytes(value);
  ASSERT(length != 0, "Value out of range");

  char* heapChars = ALLOCATE(char, length + 1);
  heapChars[length] = '\0';
  utf8Encode(value, (uint8_t*)heapChars);

  return takeString(heapChars, length);
}

ObjString* stringFromByte(uint8_t byte) {
  char* heapChars = ALLOCATE(char, 2);
  heapChars[0] = byte;
  heapChars[1] = '\0';

  return takeString(heapChars, 1);
}

ObjString* stringFromRange(ObjString* string, uint32_t start, uint32_t count, int step) {
  uint8_t* from = (uint8_t*)string->chars;
  int length = 0;
  for (uint32_t i = 0; i < count; i++) {
    length += utf8DecodeNumBytes(from[start + i * step]);
  }

  char* heapChars = ALLOCATE(char, length + 1);
  heapChars[length] = '\0';

  uint8_t* to = (uint8_t*)heapChars;
  for (uint32_t i = 0; i < count; i++) {
    int index = start + i * step;
    int codePoint = utf8Decode(from + index, string->length - index);

    if (codePoint != -1) {
      to += utf8Encode(codePoint, to);
    }
  }

  return takeString(heapChars, length);
}

ObjString* stringFormat(const char* format, ...) {
  va_list argList;

  va_start(argList, format);
  size_t totalLength = 0;
  for (const char* c = format; *c != '\0'; c++) {
    switch (*c) {
      case '$':
        totalLength += strlen(va_arg(argList, const char*));
        break;

      case '#':
        totalLength += va_arg(argList, ObjString*)->length;
        break;

      default:
        totalLength++;
    }
  }
  va_end(argList);

  char* heapChars = ALLOCATE(char, totalLength + 1);
  heapChars[totalLength] = '\0';

  va_start(argList, format);
  char* start = heapChars;
  for (const char* c = format; *c != '\0'; c++) {
    switch (*c) {
      case '$': {
        const char* string = va_arg(argList, const char*);
        size_t length = strlen(string);
        memcpy(start, string, length);
        start += length;
        break;
      }

      case '#': {
        ObjString* string = va_arg(argList, ObjString*);
        memcpy(start, string->chars, string->length);
        start += string->length;
        break;
      }

      default:
        *start++ = *c;
    }
  }
  va_end(argList);

  return takeString(heapChars, totalLength);
}

ObjString* stringCodePointAt(ObjString* string, uint32_t index) {
  ASSERT(index < string->length, "Index out of bounds");
  
  int codePoint = utf8Decode((uint8_t*)string->chars + index, string->length - index);

  if (codePoint == -1) {
    return copyStringLength(string->chars + index, 1);
  }

  return stringFromCodePoint(codePoint);
}

uint32_t stringFind(ObjString* string, ObjString* search, uint32_t start) {
  if (search->length == 0) return start;

  if (start + search->length > string->length) return UINT32_MAX;

  if (start > string->length) return UINT32_MAX;

  uint32_t shift[UINT8_MAX];
  uint32_t searchEnd = search->length - 1;

  for (uint32_t index = 0; index < UINT8_MAX; index++) {
    shift[index] = search->length;
  }

  for (uint32_t index = 0; index < searchEnd; index++) {
    char c = search->chars[index];
    shift[(uint8_t)c] = searchEnd - index;
  }

  char lastChar = search->chars[searchEnd];
  uint32_t range = string->length - search->length;

  for (uint32_t index = start; index <= range;) {
    char c = string->chars[index + searchEnd];
    if (lastChar == c && memcmp(string->chars + index, search->chars, searchEnd) == 0) {
      return index;
    }

    index += shift[(uint8_t)c];
  }

  return UINT32_MAX;
}

ObjTuple* newTuple(int count) {
  ObjTuple* tuple = ALLOCATE_OBJ(ObjTuple, OBJ_TUPLE, vm.tupleClass);
  tuple->count = count;
  tuple->items = ALLOCATE(Value, count);
  return tuple;
}

ObjUpvalue* newUpvalue(Value* slot) {
  ObjUpvalue* upvalue = ALLOCATE_OBJ(ObjUpvalue, OBJ_UPVALUE, NULL);
  upvalue->closed = NONE_VAL;
  upvalue->location = slot;
  upvalue->next = NULL;
  return upvalue;
}

static void printFunction(ObjFunction* function, const char* type) {
  if (function->name == NULL) {
    printf("%s", function->module->name->chars);
    return;
  }
  printf("<%s %s>", type, function->name->chars);
}

void printObject(Value value) {
  switch (OBJ_TYPE(value)) {
    case OBJ_BOUND_METHOD: {
      ObjBoundMethod* bound = AS_BOUND_METHOD(value);
      if (bound->isNative) printf("<native method>");
      else printFunction(bound->as.closure->function, "method");
      break;
    }
    case OBJ_CLASS:
      printf("%s", AS_CLASS(value)->name->chars);
      break;
    case OBJ_CLOSURE:
      printFunction(AS_CLOSURE(value)->function, "fn");
      break;
    case OBJ_FUNCTION:
      printFunction(AS_FUNCTION(value), "fn");
      break;
    case OBJ_INSTANCE:
      printf("%s instance", AS_INSTANCE(value)->obj.cls->name->chars);
      break;
    case OBJ_LIST: {
      ObjList* list = AS_LIST(value);
      printf("[");
      for (int i = 0; i < list->count; i++) {
        printValue(list->items[i]);
        if (i != list->count - 1) printf(", ");
      }
      printf("]");
      break;
    }
    case OBJ_MAP: {
      ObjMap* map = AS_MAP(value);
      Table table = map->table;
      printf("[");
      bool first = true;
      for (int i = 0; i < table.capacity; i++) {
        if (table.entries[i].key != NULL) {
          if (!first) printf(", ");
          first = false;
          printf("%s -> ", table.entries[i].key->chars);
          printValue(table.entries[i].value);
        }
      }
      printf("]");
      break;
    }
    case OBJ_MODULE:
      printf("module");
      break;
    case OBJ_NATIVE:
      printf("<native fn>");
      break;
    case OBJ_PRNG:
      printf("Random instance");
      break;
    case OBJ_RANGE: {
      ObjRange* range = AS_RANGE(value);
      printValue(NUMBER_VAL(range->from));
      printf("%s", range->isInclusive ? ".." : "..<");
      printValue(NUMBER_VAL(range->to));
      break;
    }
    case OBJ_STRING:
      printf("%s", AS_CSTRING(value));
      break;
    case OBJ_TUPLE: {
      ObjTuple* tuple = AS_TUPLE(value);
      printf("(");
      for (int i = 0; i < tuple->count; i++) {
        printValue(tuple->items[i]);
        if (i != tuple->count - 1) printf(", ");
      }
      printf(")");
      break;
    }
    case OBJ_UPVALUE:
      printf("upvalue");
      break;
  }
}
