#ifndef flicker_primitive_h
#define flicker_primitive_h

#include <stdarg.h>

#include "value.h"
#include "vm.h"

#define PRIMITIVE(cls, name, function) \
  tableSet(&cls->methods, copyString(name), OBJ_VAL(newPrimitive(prim_##function)))

#define DEF_PRIMITIVE(name) static bool prim_##name(Value* args)

#define RETURN_VAL(value) \
  do {                    \
    args[0] = value;      \
    return true;          \
  } while (false)

#define RETURN_OBJ(obj)     RETURN_VAL(OBJ_VAL(obj))
#define RETURN_NUMBER(num)  RETURN_VAL(NUMBER_VAL(num))
#define RETURN_BOOL(value)  RETURN_VAL(BOOL_VAL(value))
#define RETURN_NONE         RETURN_VAL(NONE_VAL)
#define RETURN_TRUE         RETURN_VAL(TRUE_VAL)
#define RETURN_FALSE        RETURN_VAL(FALSE_VAL)

#define ERROR(msg)

#define ERROR_FORMAT(...)

#define RETURN_ERROR(msg) \
  do {                    \
    return false;         \
  } while (false)

#define RETURN_ERROR_FORMAT(...) \
  do {                           \
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
