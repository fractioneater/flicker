#ifndef flicker_native_h
#define flicker_native_h

#include <stdarg.h>

#include "value.h"
#include "vm.h"

#define NATIVE(cls, name, arity, function) \
  tableSet(&cls->methods, copyString(name), OBJ_VAL(newNative(native_##function, arity)), true)

#define NATIVE_INIT(cls, function, argCount)                            \
  do {                                                                  \
    cls->initializer = OBJ_VAL(newNative(native_##function, argCount)); \
    cls->arity = argCount;                                              \
  } while (false)

#define DEF_NATIVE(name) static bool native_##name(Value* args)

#define RETURN_VAL(value) \
  do {                    \
    args[0] = value;      \
    return true;          \
  } while (false)

#define RETURN_OBJ(obj)     RETURN_VAL(OBJ_VAL(obj))
#define RETURN_NUMBER(num)  RETURN_VAL(NUMBER_VAL(num))
#define RETURN_BOOL(value)  RETURN_VAL(BOOL_VAL(value))
#define RETURN_NONE()       RETURN_VAL(NONE_VAL)
#define RETURN_TRUE()       RETURN_VAL(TRUE_VAL)
#define RETURN_FALSE()      RETURN_VAL(FALSE_VAL)

#define ERROR(...) runtimeError(__VA_ARGS__)

#define RETURN_ERROR(...) \
  do {                           \
    runtimeError(__VA_ARGS__);   \
    return false;                \
  } while (false)

bool validateNumber(Value arg, const char* argName);

bool validateIntValue(double value, const char* argName);
bool validateInt(Value arg, const char* argName);

uint32_t validateIndex(Value arg, uint32_t count, const char* argName);

bool validateFunction(Value arg, const char* argName);

bool validateString(Value arg, const char* argName);

uint32_t calculateRange(ObjRange* range, uint32_t* length, int* step);

#endif
