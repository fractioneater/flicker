#ifndef flicker_object_h
#define flicker_object_h

#include "chunk.h"
#include "common.h"
#include "shishua.h"
#include "table.h"
#include "value.h"

#define OBJ_TYPE(value)        (AS_OBJ(value)->type)

#define IS_BOUND_METHOD(value) isObjType(value, OBJ_BOUND_METHOD)
#define IS_CLASS(value)        isObjType(value, OBJ_CLASS)
#define IS_CLOSURE(value)      isObjType(value, OBJ_CLOSURE)
#define IS_FUNCTION(value)     isObjType(value, OBJ_FUNCTION)
#define IS_INSTANCE(value)     isObjType(value, OBJ_INSTANCE)
#define IS_LIST(value)         isObjType(value, OBJ_LIST)
#define IS_MAP(value)          isObjType(value, OBJ_MAP)
#define IS_MODULE(value)       isObjType(value, OBJ_MODULE)
#define IS_NATIVE(value)       isObjType(value, OBJ_NATIVE)
#define IS_PRNG(value)         isObjType(value, OBJ_PRNG)
#define IS_RANGE(value)        isObjType(value, OBJ_RANGE)
#define IS_STRING(value)       isObjType(value, OBJ_STRING)

#define AS_BOUND_METHOD(value) ((ObjBoundMethod*)AS_OBJ(value))
#define AS_CLASS(value)        ((ObjClass*)AS_OBJ(value))
#define AS_CLOSURE(value)      ((ObjClosure*)AS_OBJ(value))
#define AS_FUNCTION(value)     ((ObjFunction*)AS_OBJ(value))
#define AS_INSTANCE(value)     ((ObjInstance*)AS_OBJ(value))
#define AS_LIST(value)         ((ObjList*)AS_OBJ(value))
#define AS_MAP(value)          ((ObjMap*)AS_OBJ(value))
#define AS_MODULE(value)       ((ObjModule*)AS_OBJ(value))
#define AS_NATIVE(value)       ((ObjNative*)AS_OBJ(value))
#define AS_PRNG(value)         ((ObjPrng*)AS_OBJ(value))
#define AS_RANGE(value)        ((ObjRange*)AS_OBJ(value))
#define AS_STRING(value)       ((ObjString*)AS_OBJ(value))
#define AS_CSTRING(value)      (((ObjString*)AS_OBJ(value))->chars)

typedef enum {
  OBJ_BOUND_METHOD,
  OBJ_CLASS,
  OBJ_CLOSURE,
  OBJ_FUNCTION,
  OBJ_INSTANCE,
  OBJ_LIST,
  OBJ_MAP,
  OBJ_MODULE,
  OBJ_NATIVE,
  OBJ_PRNG,
  OBJ_RANGE,
  OBJ_STRING,
  OBJ_UPVALUE
} ObjType;

typedef struct ObjClass ObjClass;

struct Obj {
  ObjType type;
  bool isMarked;
  ObjClass* cls;
  struct Obj* next;
};

typedef struct {
  Obj obj;
  Table variables;
  ObjString* name;
  bool isCore;
} ObjModule;

typedef struct {
  Obj obj;
  uint8_t arity;
  int upvalueCount;
  Chunk chunk;
  ObjString* name;
  ObjModule* module;
} ObjFunction;

typedef bool (*NativeFn)(Value* args);

typedef struct {
  Obj obj;
  NativeFn function;
} ObjNative;

typedef struct {
  Obj obj;
  uint32_t count;
  uint32_t capacity;
  Value* items;
} ObjList;

typedef struct {
  Obj obj;
  int count;
  Table table;
} ObjMap;

struct ObjString {
  Obj obj;
  int length;
  char* chars;
  uint32_t hash;
};

typedef struct {
  Obj obj;
  double from;
  double to;
  bool isInclusive;
} ObjRange;

typedef struct {
  Obj obj;
  PrngState state;
  uint8_t buffer[PRNG_BUFFER_SIZE];
  size_t bufferIndex;
} ObjPrng;

typedef struct ObjUpvalue {
  Obj obj;
  Value* location;
  Value closed;
  struct ObjUpvalue* next;
} ObjUpvalue;

typedef struct {
  Obj obj;
  ObjFunction* function;
  ObjUpvalue** upvalues;
  int upvalueCount;
} ObjClosure;

struct ObjClass {
  Obj obj;
  ObjClass* superclass;
  ObjString* name;
  Value initializer;
  uint8_t arity;
  Table methods;
};

typedef struct {
  Obj obj;
  Table fields;
} ObjInstance;

typedef struct {
  Obj obj;
  Value receiver;
  ObjClosure* method;
} ObjBoundMethod;

ObjBoundMethod* newBoundMethod(Value receiver, ObjClosure* method);

ObjClass* newSingleClass(ObjString* name);
ObjClass* newClass(ObjString* name);
void bindSuperclass(ObjClass* subclass, ObjClass* superclass);

ObjClosure* newClosure(ObjFunction* function);

ObjFunction* newFunction(ObjModule* module);

ObjInstance* newInstance(ObjClass* cls);

ObjList* newList(uint32_t count);
void listClear(ObjList* list);
void listAppend(ObjList* list, Value value);
void listInsertAt(ObjList* list, uint32_t index, Value value);
Value listDeleteAt(ObjList* list, uint32_t index);
int listIndexOf(ObjList* list, Value value);

ObjMap* newMap();
Value mapGet(ObjMap* map, Value key);
void mapSet(ObjMap* map, Value key, Value value);
void mapClear(ObjMap* map);
void mapRemoveKey(ObjMap* map, Value key);

ObjModule* newModule(ObjString* name, bool isCore);

ObjNative* newNative(NativeFn function);

ObjPrng* newPrng(uint64_t seed[4]);
void fillPrngBuffer(ObjPrng* prng);

ObjRange* newRange(double from, double to, bool isInclusive);

ObjString* takeString(char* chars, int length);
ObjString* copyStringLength(const char* chars, int length);
ObjString* copyString(const char* chars);
const char* numberToCString(double value);
ObjString* numberToString(double value);
ObjString* stringFromCodePoint(int value);
ObjString* stringFromByte(uint8_t byte);
ObjString* stringFromRange(ObjString* string, uint32_t start, uint32_t count, int step);
ObjString* stringFormat(const char* format, ...);
ObjString* stringCodePointAt(ObjString* string, uint32_t index);
uint32_t stringFind(ObjString* string, ObjString* search, uint32_t start);

ObjUpvalue* newUpvalue(Value* slot);

void printObject(Value value);

static inline void fillPrng(ObjPrng* prng, uint8_t* buffer, size_t size) {
  size_t bytesLeft = size, bytesFilled = 0, chunkSize;
  while (bytesLeft > 0) {
    chunkSize = PRNG_BUFFER_SIZE - prng->bufferIndex;
    if (chunkSize > bytesLeft) {
      chunkSize = bytesLeft;
    }
    memcpy(&buffer[bytesFilled], &prng->buffer[prng->bufferIndex], chunkSize);
    prng->bufferIndex += chunkSize;
    bytesFilled += chunkSize;
    bytesLeft -= chunkSize;
    if (prng->bufferIndex >= PRNG_BUFFER_SIZE) {
      fillPrngBuffer(prng);
    }
  }
}

static inline bool isValidListIndex(ObjList* list, uint32_t index) {
  return (index < 0) ? (-list->count <= index) : (index <= list->count - 1);
}

static inline bool isValidStringIndex(ObjString* string, int index) {
  return (index < 0) ? (-string->length <= index) : (index <= string->length - 1);
}

static inline bool stringEqualsCString(ObjString* a, const char* b, size_t length) {
  return a->length == length && memcmp(a->chars, b, length) == 0;
}

static inline bool isObjType(Value value, ObjType type) {
  return IS_OBJ(value) && AS_OBJ(value)->type == type;
}

#endif
