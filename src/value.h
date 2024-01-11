#pragma once

#include <string.h>

#include "common.h"

typedef struct Obj Obj;
typedef struct ObjString ObjString;

static inline double numFromBits(uint64_t value) {
  double num;
  memcpy(&num, &value, sizeof(uint64_t));
  return num;
}

static inline uint64_t numToBits(double num) {
  uint64_t value;
  memcpy(&value, &num, sizeof(double));
  return value;
}

#define QNAN_MIN_BITS ((uint64_t)0x7ff8000000000000)
#define DOUBLE_NAN    (numFromBits(QNAN_MIN_BITS))

#if NAN_TAGGING

#  define SIGN_BIT ((uint64_t)0x8000000000000000)
#  define QNAN     ((uint64_t)0x7ffc000000000000)

#  define MASK_TAG      (7)

#  define TAG_NAN       (0)
#  define TAG_NONE      (1)
#  define TAG_FALSE     (2)
#  define TAG_TRUE      (3)
#  define TAG_UNDEFINED (4)
#  define TAG_UNUSED2   (5)
#  define TAG_UNUSED3   (6)
#  define TAG_UNUSED4   (7)

typedef uint64_t Value;

#  define GET_TAG(value)      ((int)((value) & MASK_TAG))

#  define IS_BOOL(value)      (((value) | 1) == TRUE_VAL)
#  define IS_NONE(value)      ((value) == NONE_VAL)
#  define IS_UNDEFINED(value) ((value) == UNDEFINED_VAL)
#  define IS_NUMBER(value)    (((value) & QNAN) != QNAN)
#  define IS_OBJ(value)       (((value) & (QNAN | SIGN_BIT)) == (QNAN | SIGN_BIT))

#  define AS_BOOL(value)      ((value) == TRUE_VAL)
#  define AS_NUMBER(value)    numFromBits(value)
#  define AS_OBJ(value)       ((Obj*)(uintptr_t)((value) & ~(SIGN_BIT | QNAN)))

#  define BOOL_VAL(b)         ((b) ? TRUE_VAL : FALSE_VAL)
#  define FALSE_VAL           ((Value)(uint64_t)(QNAN | TAG_FALSE))
#  define TRUE_VAL            ((Value)(uint64_t)(QNAN | TAG_TRUE))
#  define NONE_VAL            ((Value)(uint64_t)(QNAN | TAG_NONE))
#  define UNDEFINED_VAL       ((Value)(uint64_t)(QNAN | TAG_UNDEFINED))
#  define NUMBER_VAL(num)     ((Value)numToBits(num))
#  define OBJ_VAL(obj)        (Value)(SIGN_BIT | QNAN | (uint64_t)(uintptr_t)(obj))

#else

typedef enum {
  VAL_BOOL,
  VAL_NONE,
  VAL_UNDEFINED,
  VAL_NUMBER,
  VAL_OBJ
} ValueType;

typedef struct {
  ValueType type;
  union {
    bool boolean;
    double number;
    Obj* obj;
  } as;
} Value;

#  define IS_BOOL(value)      ((value).type == VAL_BOOL)
#  define IS_NONE(value)      ((value).type == VAL_NONE)
#  define IS_UNDEFINED(value) ((value).type == VAL_UNDEFINED)
#  define IS_NUMBER(value)    ((value).type == VAL_NUMBER)
#  define IS_OBJ(value)       ((value).type == VAL_OBJ)

#  define AS_OBJ(value)       ((value).as.obj)
#  define AS_BOOL(value)      ((value).as.boolean)
#  define AS_NUMBER(value)    ((value).as.number)

#  define BOOL_VAL(value)     ((Value){VAL_BOOL, {.boolean = value}})
#  define FALSE_VAL           (BOOL_VAL(false))
#  define TRUE_VAL            (BOOL_VAL(true))
#  define NONE_VAL            ((Value){VAL_NONE, {.number = 0}})
#  define UNDEFINED_VAL       ((Value){VAL_UNDEFINED, {.number = 0}})
#  define NUMBER_VAL(value)   ((Value){VAL_NUMBER, {.number = value}})
#  define OBJ_VAL(object)     ((Value){VAL_OBJ, {.obj = (Obj*)object}})

#endif

typedef struct {
  int capacity;
  int count;
  Value* values;
} ValueArray;

bool valuesEqual(Value a, Value b);
void initValueArray(ValueArray* array);
void writeValueArray(ValueArray* array, Value value);
void freeValueArray(ValueArray* array);
void printValue(Value value);
