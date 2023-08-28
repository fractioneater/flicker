#include "object.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "memory.h"
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

#if DEBUG_LOG_GC
  printf("%p allocate %zu for %d\n", (void*)obj, size, type);
#endif

  return obj;
}

ObjBoundMethod* newBoundMethod(Value receiver, ObjClosure* method) {
  ObjBoundMethod* bound = ALLOCATE_OBJ(ObjBoundMethod, OBJ_BOUND_METHOD, NULL);
  bound->receiver = receiver;
  bound->method = method;
  return bound;
}

ObjClass* newSingleClass(ObjString* name) {
  ObjClass* cls = ALLOCATE_OBJ(ObjClass, OBJ_CLASS, NULL);
  cls->name = name;
  cls->superclass = NULL;
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
  subclass->superclass = superclass;
  tableAddAll(&superclass->methods, &subclass->methods);
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

ObjFunction* newFunction() {
  ObjFunction* function = ALLOCATE_OBJ(ObjFunction, OBJ_FUNCTION, vm.functionClass);
  function->arity = 0;
  function->upvalueCount = 0;
  function->name = NULL;
  initChunk(&function->chunk);
  return function;
}

ObjInstance* newInstance(ObjClass* cls) {
  ObjInstance* instance = ALLOCATE_OBJ(ObjInstance, OBJ_INSTANCE, cls);
  initTable(&instance->fields);
  return instance;
}

ObjNative* newNative(NativeFn function) {
  ObjNative* obj = ALLOCATE_OBJ(ObjNative, OBJ_NATIVE, NULL);
  obj->isPrimitive = false;
  obj->as.native = function;
  return obj;
}

ObjNative* newPrimitive(Primitive function) {
  ObjNative* obj = ALLOCATE_OBJ(ObjNative, OBJ_NATIVE, NULL);
  obj->isPrimitive = true;
  obj->as.primitive = function;
  return obj;
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

static ObjString* allocateString(char* chars, int length, uint32_t hash) {
  ObjString* string = ALLOCATE_OBJ(ObjString, OBJ_STRING, vm.stringClass);
  string->length = length;
  string->chars = chars;
  string->hash = hash;

  push(OBJ_VAL(string));
  tableSet(&vm.strings, string, NONE_VAL);
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
  uint32_t hash = hashString(chars, length);
  ObjString* interned = tableFindString(&vm.strings, chars, length, hash);
  if (interned != NULL) return interned;

  char* heapChars = ALLOCATE(char, length + 1);
  memcpy(heapChars, chars, length);
  heapChars[length] = '\0';
  return allocateString(heapChars, length, hash);
}

ObjString* copyString(const char* chars) {
  return copyStringLength(chars, (int)strlen(chars));
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

  char* heapChars = ALLOCATE(char, length + 1);
  heapChars[length] = '\0';
  utf8Encode(value, (uint8_t*)heapChars);

  uint32_t hash = hashString(heapChars, length);

  return allocateString(heapChars, length, hash);
}

ObjString* stringFromByte(uint8_t byte) {
  char* heapChars = ALLOCATE(char, 2);
  heapChars[0] = byte;
  heapChars[1] = '\0';

  uint32_t hash = hashString(heapChars, 1);

  return allocateString(heapChars, 1, hash);
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

  uint32_t hash = hashString(heapChars, length);

  return allocateString(heapChars, length, hash);
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

  uint32_t hash = hashString(heapChars, totalLength);

  return allocateString(heapChars, totalLength, hash);
}

ObjString* stringCodePointAt(ObjString* string, uint32_t index) {
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

Value indexFromString(ObjString* string, int index) {
  const char* character = string->chars + index;

  int codePoint = utf8Decode((uint8_t*)character, string->length - index);

  if (codePoint == -1) {
    return OBJ_VAL(copyStringLength(character, 1));
  } else {
    int length = utf8EncodeNumBytes(codePoint);

    char* new = ALLOCATE(char, length + 1);
    new[length] = '\0';

    utf8Encode(codePoint, (uint8_t*)new);
    return OBJ_VAL(takeString(new, length));
  }
}

ObjRange* newRange(double from, double to, bool isInclusive) {
  ObjRange* range = ALLOCATE_OBJ(ObjRange, OBJ_RANGE, vm.rangeClass);
  range->from = from;
  range->to = to;
  range->isInclusive = isInclusive;
  return range;
}

ObjUpvalue* newUpvalue(Value* slot) {
  ObjUpvalue* upvalue = ALLOCATE_OBJ(ObjUpvalue, OBJ_UPVALUE, NULL);
  upvalue->closed = NONE_VAL;
  upvalue->location = slot;
  upvalue->next = NULL;
  return upvalue;
}

static void printFunction(ObjFunction* function) {
  if (function->name == NULL) {
    printf("<script>");
    return;
  }
  printf("<fn %s>", function->name->chars);
}

void printObject(Value value) {
  switch (OBJ_TYPE(value)) {
    case OBJ_BOUND_METHOD:
      printFunction(AS_BOUND_METHOD(value)->method->function);
      break;
    case OBJ_CLASS:
      printf("%s", AS_CLASS(value)->name->chars);
      break;
    case OBJ_CLOSURE:
      printFunction(AS_CLOSURE(value)->function);
      break;
    case OBJ_FUNCTION:
      printFunction(AS_FUNCTION(value));
      break;
    case OBJ_INSTANCE:
      printf("%s instance", AS_INSTANCE(value)->obj.cls->name->chars);
      break;
    case OBJ_NATIVE:
      printf("<native fn>");
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
    case OBJ_STRING:
      printf("%s", AS_CSTRING(value));
      break;
    case OBJ_RANGE: {
      ObjRange* range = AS_RANGE(value);
      printValue(NUMBER_VAL(range->from));
      printf("%s", range->isInclusive ? ".." : ":");
      printValue(NUMBER_VAL(range->to));
      break;
    }
    case OBJ_UPVALUE:
      printf("upvalue");
      break;
  }
}
