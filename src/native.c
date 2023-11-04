#include "native.h"

#include <math.h>
#include <stdlib.h>

static uint32_t validateIndexValue(uint32_t count, double value, const char* argName) {
  if (!validateIntValue(value, argName)) return UINT32_MAX;

  if (value < 0) value = count + value;

  if (0 <= value && value < count) return (uint32_t)value;

  ERROR("%s out of bounds", argName);
  return UINT32_MAX;
}

bool validateNumber(Value arg, const char* argName) {
  if (IS_NUMBER(arg)) return true;
  RETURN_ERROR("%s must be a number", argName);
}

bool validateIntValue(double value, const char* argName) {
  if (trunc(value) == value) return true;
  RETURN_ERROR("%s must be an integer", argName);
}

bool validateInt(Value arg, const char* argName) {
  if (!validateNumber(arg, argName)) return false;
  return validateIntValue(AS_NUMBER(arg), argName);
}

uint32_t validateIndex(Value arg, uint32_t count, const char* argName) {
  if (!validateNumber(arg, argName)) return UINT32_MAX;
  return validateIndexValue(count, AS_NUMBER(arg), argName);
}

bool validateFunction(Value arg, const char* argName) {
  if (IS_CLOSURE(arg)) return true;
  RETURN_ERROR("%s must be a function", argName);
  return false;
}

bool validateString(Value arg, const char* argName) {
  if (IS_STRING(arg)) return true;
  RETURN_ERROR("%s must be a string", argName);
  return false;
}

uint32_t calculateRange(ObjRange* range, uint32_t* length, int* step) {
  *step = 0;

  if (range->from == *length && range->to == (range->isInclusive ? -1.0 : (double)*length)) {
    *length = 0;
    return 0;
  }

  uint32_t from = validateIndexValue(*length, range->from, "Range start");
  if (from == UINT32_MAX) return UINT32_MAX;

  double value = range->to;
  if (!validateIntValue(value, "Range end")) return UINT32_MAX;

  if (value < 0) value = *length + value;

  if (!range->isInclusive) {
    if (value == from) {
      *length = 0;
      return from;
    }

    value += value >= from ? -1 : 1;
  }

  if (value <= 0 || value >= *length) {
    ERROR("Range end out of bounds");
    return UINT32_MAX;
  }

  uint32_t to = (uint32_t)value;
  *length = abs((int)(from - to)) + 1;
  *step = from < to ? 1 : -1;
  return from;
}
