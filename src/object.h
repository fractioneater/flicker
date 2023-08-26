#ifndef flicker_object_h
#define flicker_object_h

#include "chunk.h"
#include "common.h"
#include "table.h"
#include "value.h"

#define OBJ_TYPE(value)        (AS_OBJ(value)->type)

#define IS_BOUND_METHOD(value) isObjType(value, OBJ_BOUND_METHOD)
#define IS_CLASS(value)        isObjType(value, OBJ_CLASS)
#define IS_CLOSURE(value)      isObjType(value, OBJ_CLOSURE)
#define IS_FUNCTION(value)     isObjType(value, OBJ_FUNCTION)
#define IS_INSTANCE(value)     isObjType(value, OBJ_INSTANCE)
#define IS_NATIVE(value)       isObjType(value, OBJ_NATIVE)
#define IS_LIST(value)         isObjType(value, OBJ_LIST)
#define IS_STRING(value)       isObjType(value, OBJ_STRING)
#define IS_RANGE(value)        isObjType(value, OBJ_RANGE)

#define AS_BOUND_METHOD(value) ((ObjBoundMethod*)AS_OBJ(value))
#define AS_CLASS(value)        ((ObjClass*)AS_OBJ(value))
#define AS_CLOSURE(value)      ((ObjClosure*)AS_OBJ(value))
#define AS_FUNCTION(value)     ((ObjFunction*)AS_OBJ(value))
#define AS_INSTANCE(value)     ((ObjInstance*)AS_OBJ(value))
#define AS_NATIVE(value)       (((ObjNative*)AS_OBJ(value)))
#define AS_LIST(value)         ((ObjList*)AS_OBJ(value))
#define AS_STRING(value)       ((ObjString*)AS_OBJ(value))
#define AS_CSTRING(value)      (((ObjString*)AS_OBJ(value))->chars)
#define AS_RANGE(value)        ((ObjRange*)AS_OBJ(value))

typedef enum {
  OBJ_BOUND_METHOD,
  OBJ_CLASS,
  OBJ_CLOSURE,
  OBJ_FUNCTION,
  OBJ_INSTANCE,
  OBJ_NATIVE,
  OBJ_LIST,
  OBJ_STRING,
  OBJ_RANGE,
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
  int arity;
  int upvalueCount;
  Chunk chunk;
  ObjString* name;
} ObjFunction;

typedef bool (*Primitive)(Value* args);
typedef Value (*NativeFn)(int argCount, Value* args);

typedef struct {
  Obj obj;
  bool isPrimitive;
  
  union {
    NativeFn native;
    Primitive primitive;
  } as;
} ObjNative;

typedef struct {
  Obj obj;
  uint32_t count;
  uint32_t capacity;
  Value* items;
} ObjList;

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
  Table methods;
  Table classMethods;
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

ObjClass* newClass(ObjString* name);
void bindSuperclass(ObjClass* subclass, ObjClass* superclass);

ObjClosure* newClosure(ObjFunction* function);

ObjFunction* newFunction();

ObjInstance* newInstance(ObjClass* cls);

ObjNative* newNative(NativeFn function);
ObjNative* newPrimitive(Primitive function);

ObjList* newList(uint32_t count);
void listClear(ObjList* list);
void listAppend(ObjList* list, Value value);
void listInsertAt(ObjList* list, uint32_t index, Value value);
Value listDeleteAt(ObjList* list, uint32_t index);
int listIndexOf(ObjList* list, Value value);

ObjString* takeString(char* chars, int length);
ObjString* copyStringLength(const char* chars, int length);
ObjString* copyString(const char* chars);
ObjString* numberToString(double value);
ObjString* stringFromCodePoint(int value);
ObjString* stringFromByte(uint8_t byte);
ObjString* stringFromRange(ObjString* string, uint32_t start, uint32_t count, int step);
ObjString* stringFormat(const char* format, ...);
ObjString* stringCodePointAt(ObjString* string, uint32_t index);
uint32_t stringFind(ObjString* string, ObjString* search, uint32_t start);
Value indexFromString(ObjString* string, int index);

ObjRange* newRange(double from, double to, bool isInclusive);

ObjUpvalue* newUpvalue(Value* slot);

void printObject(Value value);

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
