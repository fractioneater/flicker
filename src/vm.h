#ifndef flicker_vm_h
#define flicker_vm_h

#include "object.h"
#include "table.h"
#include "value.h"

#define FRAMES_MAX 64
#define STACK_MAX (FRAMES_MAX * UINT8_COUNT)

#define MAX_TEMP_ROOTS 8

typedef struct {
  ObjClosure* closure;
  uint8_t* ip;
  Value* slots;
} CallFrame;

typedef struct {
  ObjClass* objectClass;
  ObjClass* classClass;
  ObjClass* boolClass;
  ObjClass* functionClass;
  ObjClass* listClass;
  ObjClass* noneClass;
  ObjClass* numberClass;
  ObjClass* rangeClass;
  ObjClass* stringClass;

  CallFrame frames[FRAMES_MAX];
  int frameCount;

  Value stack[STACK_MAX];
  Value* stackTop;
  Table globals;
  Table strings;
  ObjString* initString;
  ObjUpvalue* openUpvalues;

  size_t bytesAllocated;
  size_t nextGC;
  Obj* objects;
  int grayCount;
  int grayCapacity;
  Obj** grayStack;

  Obj* tempRoots[MAX_TEMP_ROOTS];
  int rootCount;
} VM;

typedef enum {
  INTERPRET_OK,
  INTERPRET_COMPILE_ERROR,
  INTERPRET_RUNTIME_ERROR
} InterpretResult;

extern VM vm;

static inline ObjClass* getClass(Value value) {
  if (IS_NUMBER(value)) return vm.numberClass;
  if (IS_OBJ(value)) return AS_OBJ(value)->cls;

#if NAN_TAGGING
  switch (GET_TAG(value)) {
    case TAG_FALSE: return vm.boolClass; break;
    case TAG_TRUE:  return vm.boolClass; break;
    case TAG_NONE:  return vm.noneClass; break;
    case TAG_NAN:   return vm.numberClass; break;
  }
#else
  switch(value.type) {
    case VAL_BOOL:   return vm.boolClass;
    case VAL_NONE:   return vm.noneClass;
    case VAL_NUMBER: return vm.numberClass;
    case VAL_OBJ:    return AS_OBJ(value)->cls;
  }
#endif

  return NULL;
}

void runtimeError(const char* format, ...);
void initVM();
void freeVM();
InterpretResult interpret(const char* source, const char* module);
void push(Value value);
Value pop();

static inline void pushRoot(Obj* obj) {
  vm.tempRoots[vm.rootCount++] = obj;
}

static inline void popRoot() {
  vm.rootCount--;
}

#endif
