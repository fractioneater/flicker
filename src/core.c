#include "core.h"

#include <ctype.h>
#include <errno.h>
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#include "debug.h"
#include "memory.h"
#include "native.h"
#include "shishua.h"
#include "utils.h"
#include "value.h"

#include "core.fl.c"

///////////////////
// Bool          //
///////////////////

DEF_NATIVE(bool_not) { RETURN_BOOL(!AS_BOOL(args[0])); }

DEF_NATIVE(bool_toString) {
  if (AS_BOOL(args[0])) {
    RETURN_OBJ(copyStringLength("True", 4));
  } else {
    RETURN_OBJ(copyStringLength("False", 5));
  }
}

//////////////////////////
// BoundMethod          //
//////////////////////////

DEF_NATIVE(boundMethod_arity) {
  ObjBoundMethod* bound = AS_BOUND_METHOD(args[0]);
  if (bound->isNative) {
    RETURN_NUMBER(bound->as.native->arity);
  } else {
    RETURN_NUMBER(bound->as.closure->function->arity);
  }
}

DEF_NATIVE(boundMethod_receiver) { RETURN_VAL(AS_BOUND_METHOD(args[0])->receiver); }

DEF_NATIVE(boundMethod_toString) {
  ObjBoundMethod* bound = AS_BOUND_METHOD(args[0]);
  if (bound->isNative) RETURN_OBJ(copyStringLength("<native method>", 15));
  else RETURN_OBJ(stringFormat("<method #>", bound->as.closure->function->name));
}

////////////////////
// Class          //
////////////////////

DEF_NATIVE(class_name) { RETURN_OBJ(AS_CLASS(args[0])->name); }

DEF_NATIVE(class_supertype) {
  ObjClass* cls = AS_CLASS(args[0]);

  if (cls->superclass == NULL) RETURN_NONE();

  RETURN_OBJ(cls->superclass);
}

DEF_NATIVE(class_toString) { RETURN_OBJ(AS_CLASS(args[0])->name); }

///////////////////////
// Function          //
///////////////////////

DEF_NATIVE(function_arity) {
  RETURN_NUMBER(AS_CLOSURE(args[0])->function->arity);
}

DEF_NATIVE(function_toString) {
  ObjFunction* function = AS_CLOSURE(args[0])->function;
  RETURN_OBJ(stringFormat("<fn #>", function->name));
}

///////////////////
// List          //
///////////////////

DEF_NATIVE(list_init) { RETURN_OBJ(newList(0)); }

DEF_NATIVE(list_filled) {
  if (!validateInt(args[1], "Size")) return false;
  if (AS_NUMBER(args[1]) < 0) RETURN_ERROR("Size cannot be negative");

  uint32_t size = (uint32_t)AS_NUMBER(args[1]);
  ObjList* list = newList(size);

  for (uint32_t i = 0; i < size; i++) {
    list->items[i] = args[2];
  }

  RETURN_OBJ(list);
}

DEF_NATIVE(list_add) {
  listAppend(AS_LIST(args[0]), args[1]);
  RETURN_NONE();
}

DEF_NATIVE(list_addCore) {
  listAppend(AS_LIST(args[0]), args[1]);
  RETURN_VAL(args[0]);
}

DEF_NATIVE(list_clear) {
  listClear(AS_LIST(args[0]));
  RETURN_NONE();
}

DEF_NATIVE(list_size) { RETURN_NUMBER(AS_LIST(args[0])->count); }

DEF_NATIVE(list_insert) {
  ObjList* list = AS_LIST(args[0]);

  uint32_t index = validateIndex(args[1], list->count + 1, "Index");
  if (index == UINT32_MAX) return false;

  listInsertAt(list, index, args[2]);
  RETURN_VAL(args[2]);
}

DEF_NATIVE(list_iterate) {
  ObjList* list = AS_LIST(args[0]);

  if (IS_NONE(args[1])) {
    if (list->count == 0) RETURN_FALSE();
    RETURN_NUMBER(0);
  }

  if (!validateInt(args[1], "Iterator")) return false;

  double index = AS_NUMBER(args[1]);
  if (index < 0 || index >= list->count - 1) RETURN_FALSE();

  RETURN_NUMBER(index + 1);
}

DEF_NATIVE(list_iteratorValue) {
  ObjList* list = AS_LIST(args[0]);
  uint32_t index = validateIndex(args[1], list->count, "Iterator");
  if (index == UINT32_MAX) return false;

  RETURN_VAL(list->items[index]);
}

DEF_NATIVE(list_removeAt) {
  ObjList* list = AS_LIST(args[0]);
  uint32_t index = validateIndex(args[1], list->count, "Index");
  if (index == UINT32_MAX) return false;

  RETURN_VAL(listDeleteAt(list, index));
}

DEF_NATIVE(list_removeValue) {
  ObjList* list = AS_LIST(args[0]);
  int index = listIndexOf(list, args[1]);
  if (index == -1) RETURN_NONE();
  RETURN_VAL(listDeleteAt(list, index));
}

DEF_NATIVE(list_indexOf) {
  ObjList* list = AS_LIST(args[0]);
  int index = listIndexOf(list, args[1]);
  if (index == -1) RETURN_NONE();
  RETURN_NUMBER(index);
}

DEF_NATIVE(list_swap) {
  ObjList* list = AS_LIST(args[0]);
  uint32_t indexA = validateIndex(args[1], list->count, "Index 0");
  if (indexA == UINT32_MAX) return false;
  uint32_t indexB = validateIndex(args[2], list->count, "Index 1");
  if (indexB == UINT32_MAX) return false;

  Value a = list->items[indexA];
  list->items[indexA] = list->items[indexB];
  list->items[indexB] = a;

  RETURN_NONE();
}

DEF_NATIVE(list_get) {
  ObjList* list = AS_LIST(args[0]);

  if (IS_NUMBER(args[1])) {
    uint32_t index = validateIndex(args[1], list->count, "Index");
    if (index == UINT32_MAX) return false;

    RETURN_VAL(list->items[index]);
  }

  if (!IS_RANGE(args[1])) {
    RETURN_ERROR("Subscript must be a number or a range");
  }

  int step;
  uint32_t count = list->count;
  uint32_t start = calculateRange(AS_RANGE(args[1]), &count, &step);
  if (start == UINT32_MAX) return false;

  ObjList* result = newList(count);
  for (uint32_t i = 0; i < count; i++) {
    result->items[i] = list->items[start + i * step];
  }

  RETURN_OBJ(result);
}

DEF_NATIVE(list_set) {
  ObjList* list = AS_LIST(args[0]);
  uint32_t index = validateIndex(args[1], list->count, "Index");
  if (index == UINT32_MAX) return false;

  list->items[index] = args[2];
  RETURN_VAL(args[2]);
}

//////////////////
// Map          //
//////////////////

DEF_NATIVE(map_init) { RETURN_OBJ(newMap()); }

DEF_NATIVE(map_get) {
  // TODO MAYBE: Support more than just strings as keys.
  if (!validateString(args[1], "Key")) return false;

  ObjMap* map = AS_MAP(args[0]);
  Value value = mapGet(map, args[1]);
  if (IS_UNDEFINED(value)) RETURN_NONE();

  RETURN_VAL(value);
}

DEF_NATIVE(map_set) {
  if (!validateString(args[1], "Key")) return false;

  mapSet(AS_MAP(args[0]), args[1], args[2]);
  RETURN_VAL(args[2]);
}

DEF_NATIVE(map_addCore) {
  if (!validateString(args[1], "Key")) return false;

  mapSet(AS_MAP(args[0]), args[1], args[2]);
  RETURN_VAL(args[0]);
}

DEF_NATIVE(map_clear) {
  mapClear(AS_MAP(args[0]));
  RETURN_NONE();
}

DEF_NATIVE(map_containsKey) {
  if (!validateString(args[1], "Key")) return false;

  RETURN_BOOL(!IS_UNDEFINED(mapGet(AS_MAP(args[0]), args[1])));
}

DEF_NATIVE(map_size) { RETURN_NUMBER(AS_MAP(args[0])->count); }

DEF_NATIVE(map_remove) {
  if (!validateString(args[1], "Key")) return false;

  mapRemoveKey(AS_MAP(args[0]), args[1]);
  RETURN_NONE();
}

DEF_NATIVE(map_iterate) {
  ObjMap* map = AS_MAP(args[0]);

  if (map->table.count == 0) RETURN_FALSE();

  uint32_t index = 0;

  if (!IS_NONE(args[1])) {
    if (!validateInt(args[1], "Iterator")) return false;

    if (AS_NUMBER(args[1]) < 0) RETURN_FALSE();
    index = (uint32_t)AS_NUMBER(args[1]);

    if (index >= map->table.capacity) RETURN_FALSE();

    index++;
  }

  for (; index < map->table.capacity; index++) {
    if (map->table.entries[index].key != NULL) RETURN_NUMBER(index);
  }

  RETURN_FALSE();
}

DEF_NATIVE(map_keyIteratorValue) {
  ObjMap* map = AS_MAP(args[0]);
  uint32_t index = validateIndex(args[1], map->table.capacity, "Iterator");
  if (index == UINT32_MAX) return false;

  Entry* entry = &map->table.entries[index];
  if (entry->key == NULL) {
    RETURN_ERROR("Invalid map iterator");
  }

  RETURN_OBJ(entry->key);
}

DEF_NATIVE(map_valueIteratorValue) {
  ObjMap* map = AS_MAP(args[0]);
  uint32_t index = validateIndex(args[1], map->table.capacity, "Iterator");
  if (index == UINT32_MAX) return false;

  Entry* entry = &map->table.entries[index];
  if (entry->key == NULL) {
    RETURN_ERROR("Invalid map iterator");
  }

  RETURN_VAL(entry->value);
}

///////////////////
// None          //
///////////////////

DEF_NATIVE(none_not) { RETURN_TRUE(); }

DEF_NATIVE(none_toString) { RETURN_OBJ(copyStringLength("None", 4)); }

/////////////////////
// Number          //
/////////////////////

DEF_NATIVE(number_fromString) {
  if (!validateString(args[1], "Argument")) return false;

  ObjString* string = AS_STRING(args[1]);

  if (string->length == 0) RETURN_NONE();

  size_t len = string->length + 1;
  char* copy = ALLOCATE(char, len);
  memcpy(copy, string->chars, len);
  copy[len - 1] = '\0';

  char *read = copy, *write = copy;
  while (*read) {
    *write = *read++;
    write += (*write != '_');
  }
  *write = '\0';

  errno = 0;
  char* end;
  double number = strtod(copy, &end);

  while (*end != '\0' && isspace((unsigned char)*end)) end++;

  if (errno == ERANGE) {
    FREE_ARRAY(char, copy, len);
    RETURN_ERROR("Number literal is too large");
  }

  if (end < copy + strlen(copy)) {
    FREE_ARRAY(char, copy, len);
    RETURN_NONE();
  }

  FREE_ARRAY(char, copy, len);
  RETURN_NUMBER(number);
}

#define DEF_NUM_CONSTANT(name, value) DEF_NATIVE(number_##name) { RETURN_NUMBER(value); }

DEF_NUM_CONSTANT(infinity,   INFINITY)
DEF_NUM_CONSTANT(nan,        DOUBLE_NAN)
DEF_NUM_CONSTANT(pi,         3.141592653589793238462643383279502884197L)
DEF_NUM_CONSTANT(tau,        6.283185307179586476925286766559005768394L)
DEF_NUM_CONSTANT(maxDouble,  DBL_MAX)
DEF_NUM_CONSTANT(minDouble,  DBL_MIN)
DEF_NUM_CONSTANT(maxInteger, 9007199254740991.0)
DEF_NUM_CONSTANT(minInteger, -9007199254740991.0)

#define DEF_NUM_INFIX(name, op, type)                            \
  DEF_NATIVE(number_##name) {                                    \
    if (!validateNumber(args[1], "Right operand")) return false; \
    RETURN_##type(AS_NUMBER(args[0]) op AS_NUMBER(args[1]));     \
  }

DEF_NUM_INFIX(plus,     +,  NUMBER)
DEF_NUM_INFIX(minus,    -,  NUMBER)
DEF_NUM_INFIX(multiply, *,  NUMBER)
DEF_NUM_INFIX(divide,   /,  NUMBER)
DEF_NUM_INFIX(lt,       <,  BOOL)
DEF_NUM_INFIX(gt,       >,  BOOL)
DEF_NUM_INFIX(lte,      <=, BOOL)
DEF_NUM_INFIX(gte,      >=, BOOL)

#define DEF_NUM_BITWISE(name, op)                                \
  DEF_NATIVE(number_bitwise##name) {                             \
    if (!validateNumber(args[1], "Right operand")) return false; \
    uint32_t left = (uint32_t)AS_NUMBER(args[0]);                \
    uint32_t right = (uint32_t)AS_NUMBER(args[1]);               \
    RETURN_NUMBER(left op right);                                \
  }

DEF_NUM_BITWISE(Or,         |)
DEF_NUM_BITWISE(Xor,        ^)
DEF_NUM_BITWISE(And,        &)
DEF_NUM_BITWISE(LeftShift,  <<)
DEF_NUM_BITWISE(RightShift, >>)

#define DEF_NUM_FN(name, fn) \
  DEF_NATIVE(number_##name) { RETURN_NUMBER(fn(AS_NUMBER(args[0]))); }

DEF_NUM_FN(abs,    fabs)
DEF_NUM_FN(acos,   acos)
DEF_NUM_FN(asin,   asin)
DEF_NUM_FN(atan,   atan)
DEF_NUM_FN(cbrt,   cbrt)
DEF_NUM_FN(ceil,   ceil)
DEF_NUM_FN(cos,    cos)
DEF_NUM_FN(floor,  floor)
DEF_NUM_FN(negate, -)
DEF_NUM_FN(round,  round)
DEF_NUM_FN(sin,    sin)
DEF_NUM_FN(sqrt,   sqrt)
DEF_NUM_FN(tan,    tan)
DEF_NUM_FN(log,    log)
DEF_NUM_FN(log2,   log2)
DEF_NUM_FN(exp,    exp)

DEF_NATIVE(number_mod) {
  if (!validateNumber(args[1], "Right operand")) return false;
  double a = AS_NUMBER(args[0]);
  double b = AS_NUMBER(args[1]);
  double c = fmod(a, b);
  if ((c < 0 && b >= 0) || (b < 0 && a >= 0)) c += b;
  RETURN_NUMBER(c);
}

DEF_NATIVE(number_equals) {
  if (!IS_NUMBER(args[1])) RETURN_FALSE();
  RETURN_BOOL(AS_NUMBER(args[0]) == AS_NUMBER(args[1]));
}

DEF_NATIVE(number_notEquals) {
  if (!IS_NUMBER(args[1])) RETURN_TRUE();
  RETURN_BOOL(AS_NUMBER(args[0]) != AS_NUMBER(args[1]));
}

DEF_NATIVE(number_bitwiseNot) {
  RETURN_NUMBER(~(uint32_t)AS_NUMBER(args[0]));
}

DEF_NATIVE(number_rangeDotDot) {
  if (!validateNumber(args[1], "Right hand side of range")) return false;

  double from = AS_NUMBER(args[0]);
  double to = AS_NUMBER(args[1]);
  RETURN_OBJ(newRange(from, to, true));
}

DEF_NATIVE(number_rangeDotDotLess) {
  if (!validateNumber(args[1], "Right hand side of range")) return false;

  double from = AS_NUMBER(args[0]);
  double to = AS_NUMBER(args[1]);
  RETURN_OBJ(newRange(from, to, false));
}

DEF_NATIVE(number_atan2) {
  if (!validateNumber(args[1], "x value")) return false;
  RETURN_NUMBER(atan2(AS_NUMBER(args[0]), AS_NUMBER(args[1])));
}

DEF_NATIVE(number_min) {
  if (!validateNumber(args[1], "Other value")) return false;

  double a = AS_NUMBER(args[0]);
  double b = AS_NUMBER(args[1]);
  RETURN_NUMBER(a <= b ? a : b);
}

DEF_NATIVE(number_max) {
  if (!validateNumber(args[1], "Other value")) return false;

  double a = AS_NUMBER(args[0]);
  double b = AS_NUMBER(args[1]);
  RETURN_NUMBER(a >= b ? a : b);
}

DEF_NATIVE(number_clamp) {
  if (!validateNumber(args[1], "Min value")) return false;
  if (!validateNumber(args[2], "Max value")) return false;

  double value = AS_NUMBER(args[0]);
  double min = AS_NUMBER(args[1]);
  double max = AS_NUMBER(args[2]);
  RETURN_NUMBER((value < min) ? min : ((value > max) ? max : value));
}

DEF_NATIVE(number_gcd) {
  if (!validateNumber(args[1], "Other value")) return false;

  double a = AS_NUMBER(args[0]);
  double b = AS_NUMBER(args[1]);
  while ((a = fmod(a, b))) {
    double temp = b;
    b = a;
    a = temp;
  }

  RETURN_NUMBER(b);
}

DEF_NATIVE(number_lcm) {
  if (!validateNumber(args[1], "Other value")) return false;

  double a = AS_NUMBER(args[0]);
  double b = AS_NUMBER(args[1]);

  if (b == 0) RETURN_NUMBER(a);

  double newA = a;
  double newB = b;
  while ((newA = fmod(newA, newB))) {
    double temp = newB;
    newB = newA;
    newA = temp;
  }

  RETURN_NUMBER(a * b / newB);
}

DEF_NATIVE(number_pow) {
  if (!validateNumber(args[1], "Power value")) return false;
  RETURN_NUMBER(pow(AS_NUMBER(args[0]), AS_NUMBER(args[1])));
}

DEF_NATIVE(number_fraction) {
  double unused;
  RETURN_NUMBER(modf(AS_NUMBER(args[0]), &unused));
}

DEF_NATIVE(number_isInfinity) {
  RETURN_BOOL(isinf(AS_NUMBER(args[0])));
}

DEF_NATIVE(number_isInteger) {
  double value = AS_NUMBER(args[0]);
  if (isnan(value) || isinf(value)) RETURN_FALSE();
  RETURN_BOOL(trunc(value) == value);
}

DEF_NATIVE(number_isNan) {
  RETURN_BOOL(isnan(AS_NUMBER(args[0])));
}

DEF_NATIVE(number_sign) {
  double value = AS_NUMBER(args[0]);
  if (value > 0)
    RETURN_NUMBER(1);
  else if (value < 0)
    RETURN_NUMBER(-1);
  else
    RETURN_NUMBER(0);
}

DEF_NATIVE(number_toString) {
  RETURN_OBJ(numberToString(AS_NUMBER(args[0])));
}

DEF_NATIVE(number_truncate) {
  double integer;
  modf(AS_NUMBER(args[0]), &integer);
  RETURN_NUMBER(integer);
}

/////////////////////
// Object          //
/////////////////////

DEF_NATIVE(object_same) {
  RETURN_BOOL(valuesEqual(args[1], args[2]));
}

DEF_NATIVE(object_not) { RETURN_FALSE(); }

DEF_NATIVE(object_equals) {
  RETURN_BOOL(valuesEqual(args[0], args[1]));
}

DEF_NATIVE(object_notEquals) {
  RETURN_BOOL(!valuesEqual(args[0], args[1]));
}

DEF_NATIVE(object_is) {
  if (!IS_CLASS(args[1])) {
    ERROR("Right operand must be a class");
    return false;
  }

  ObjClass* cls = getClass(args[0]);
  ObjClass* baseClass = AS_CLASS(args[1]);

  do {
    if (baseClass == cls) RETURN_VAL(BOOL_VAL(true));
    cls = cls->superclass;
  } while (cls != NULL);

  RETURN_BOOL(false);
}

DEF_NATIVE(object_toString) {
  Obj* obj = AS_OBJ(args[0]);
  ObjString* name = obj->cls->name;
  RETURN_OBJ(stringFormat("# instance", name));
}

DEF_NATIVE(object_type) { RETURN_OBJ(getClass(args[0])); }

/////////////////////
// Random          //
/////////////////////

DEF_NATIVE(random_init) {
  struct timeval time;
  gettimeofday(&time, NULL);
  srand(time.tv_usec);
  uint64_t seed[4] = {rand(), rand(), rand(), rand()};
  RETURN_OBJ(newPrng(seed));
}

DEF_NATIVE(random_seed) {
  uint64_t seed[4];
  if (IS_NUMBER(args[1])) {
    if (!validateInt(args[1], "Seed")) return false;

    seed[0] = (uint64_t)AS_NUMBER(args[1]);
    seed[1] = seed[2] = seed[3] = 0;
  } else if (IS_LIST(args[1])) {
    ObjList* list = AS_LIST(args[1]);
    if (list->count != 4) RETURN_ERROR("Seed list must have 4 elements");

    for (int i = 0; i < 4; i++) {
      if (!validateInt(list->items[i], "Seed")) return false;
      seed[i] = (uint64_t)AS_NUMBER(list->items[i]);
    }
  } else if (IS_TUPLE(args[1])) {
    ObjTuple* tuple = AS_TUPLE(args[1]);
    if (tuple->count != 4) RETURN_ERROR("Seed value must have 4 elements");

    for (int i = 0; i < 4; i++) {
      if (!validateInt(tuple->items[i], "Seed")) return false;
      seed[i] = (uint64_t)AS_NUMBER(tuple->items[i]);
    }
  } else {
    RETURN_ERROR("Seed must be either a number or a list");
  }

  RETURN_OBJ(newPrng(seed));
}

DEF_NATIVE(random_randBytes) {
  ObjPrng* prng = AS_PRNG(args[0]);
  if (!validateInt(args[1], "Byte count")) return false;

  int count = (int)AS_NUMBER(args[1]);
  if (count < 0) RETURN_ERROR("Byte count must be a non-negative integer");

  ObjList* output = newList(count);
  uint8_t buffer[count];

  fillPrng(prng, buffer, count);
  for (int i = 0; i < count; i++) {
    output->items[i] = NUMBER_VAL(buffer[i]);
  }

  RETURN_OBJ(output);
}

////////////////////
// Range          //
////////////////////

DEF_NATIVE(range_init) {
  if (!validateNumber(args[1], "From value")) return false;
  if (!validateNumber(args[2], "To value")) return false;

  double from = AS_NUMBER(args[1]);
  double to = AS_NUMBER(args[2]);
  bool isInclusive = !(IS_NONE(args[3]) || (IS_BOOL(args[3]) && !AS_BOOL(args[3])));
  RETURN_OBJ(newRange(from, to, isInclusive));
}

DEF_NATIVE(range_from) { RETURN_NUMBER(AS_RANGE(args[0])->from); }

DEF_NATIVE(range_to) { RETURN_NUMBER(AS_RANGE(args[0])->to); }

DEF_NATIVE(range_min) {
  ObjRange* range = AS_RANGE(args[0]);
  RETURN_NUMBER(fmin(range->from, range->to));
}

DEF_NATIVE(range_max) {
  ObjRange* range = AS_RANGE(args[0]);
  RETURN_NUMBER(fmax(range->from, range->to));
}

DEF_NATIVE(range_isInclusive) {
  RETURN_BOOL(AS_RANGE(args[0])->isInclusive);
}

DEF_NATIVE(range_contains) {
  if (!validateNumber(args[1], "Value")) return false;

  ObjRange* range = AS_RANGE(args[0]);
  double max = fmax(range->from, range->to);
  double min = fmin(range->from, range->to);

  if (range->isInclusive) RETURN_BOOL(min <= AS_NUMBER(args[1]) && AS_NUMBER(args[1]) <= max);
  else RETURN_BOOL(min <= AS_NUMBER(args[1]) && AS_NUMBER(args[1]) < max);
}

DEF_NATIVE(range_includes) {
  if (!validateNumber(args[1], "Value")) return false;

  double value = AS_NUMBER(args[1]);
  if (trunc(value) != value) RETURN_FALSE();

  ObjRange* range = AS_RANGE(args[0]);
  double max = fmax(range->from, range->to);
  double min = fmin(range->from, range->to);

  if (range->isInclusive) RETURN_BOOL(min <= value && value <= max);
  else RETURN_BOOL(min <= value && value < max);
}

DEF_NATIVE(range_iterate) {
  ObjRange* range = AS_RANGE(args[0]);

  if (range->from == range->to && !range->isInclusive) RETURN_FALSE();

  if (IS_NONE(args[1])) RETURN_NUMBER(range->from);

  if (!validateNumber(args[1], "Iterator")) return false;
  double iterator = AS_NUMBER(args[1]);

  if (range->from < range->to) {
    iterator++;
    if (iterator > range->to) RETURN_FALSE();
  } else {
    iterator--;
    if (iterator < range->to) RETURN_FALSE();
  }

  if (!range->isInclusive && iterator == range->to) RETURN_FALSE();

  RETURN_NUMBER(iterator);
}

DEF_NATIVE(range_iteratorValue) { RETURN_VAL(args[1]); }

DEF_NATIVE(range_toString) {
  ObjRange* range = AS_RANGE(args[0]);

  ObjString* from = numberToString(range->from);
  pushRoot((Obj*)from);
  ObjString* to = numberToString(range->to);
  pushRoot((Obj*)to);

  ObjString* result = stringFormat("#$#", from, range->isInclusive ? ".." : "..<", to);

  popRoot();
  popRoot();
  RETURN_OBJ(result);
}

/////////////////////
// String          //
/////////////////////

DEF_NATIVE(string_fromCodePoint) {
  if (!validateInt(args[1], "Code point")) return false;
  int codePoint = (int)AS_NUMBER(args[1]);

  if (codePoint < 0) {
    RETURN_ERROR("Code point cannot be negative");
  } else if (codePoint > 0x10ffff) {
    RETURN_ERROR("Code point cannot be greater than 0x10ffff");
  }

  RETURN_OBJ(stringFromCodePoint(codePoint));
}

DEF_NATIVE(string_fromByte) {
  if (!validateInt(args[1], "Byte")) return false;
  int byte = (int)AS_NUMBER(args[1]);

  if (byte < 0) {
    RETURN_ERROR("Byte cannot be negative");
  } else if (byte > 0xff) {
    RETURN_ERROR("Byte cannot be greater than 0xff");
  }

  RETURN_OBJ(stringFromByte((uint8_t)byte));
}

DEF_NATIVE(string_byteAt) {
  ObjString* string = AS_STRING(args[0]);

  uint32_t index = validateIndex(args[1], string->length, "Index");
  if (index == UINT32_MAX) return false;

  RETURN_NUMBER((uint8_t)string->chars[index]);
}

DEF_NATIVE(string_byteCount) {
  RETURN_NUMBER(AS_STRING(args[0])->length);
}

DEF_NATIVE(string_codePointAt) {
  ObjString* string = AS_STRING(args[0]);

  uint32_t index = validateIndex(args[1], string->length, "Index");
  if (index == UINT32_MAX) return false;

  const uint8_t* bytes = (uint8_t*)string->chars;
  if ((bytes[index] & 0xc0) == 0x80) RETURN_NUMBER(-1);

  RETURN_NUMBER(utf8Decode((uint8_t*)string->chars + index, string->length - index));
}

DEF_NATIVE(string_concatenate) {
  if (!validateString(args[1], "Right operand")) return false;
  ObjString* a = AS_STRING(args[0]);
  ObjString* b = AS_STRING(args[1]);
  RETURN_OBJ(stringFormat("##", a, b));
}

DEF_NATIVE(string_contains) {
  if (!validateString(args[1], "Argument")) return false;

  ObjString* string = AS_STRING(args[0]);
  ObjString* search = AS_STRING(args[1]);

  RETURN_BOOL(stringFind(string, search, 0) != UINT32_MAX);
}

DEF_NATIVE(string_endsWith) {
  if (!validateString(args[1], "Argument")) return false;

  ObjString* string = AS_STRING(args[0]);
  ObjString* search = AS_STRING(args[1]);

  if (search->length > string->length) RETURN_FALSE();

  RETURN_BOOL(memcmp(string->chars + string->length - search->length,
                     search->chars, search->length) == 0);
}

DEF_NATIVE(string_get) {
  ObjString* string = AS_STRING(args[0]);

  if (IS_NUMBER(args[1])) {
    uint32_t index = validateIndex(args[1], string->length, "Subscript");
    if (index == UINT32_MAX) return false;

    RETURN_OBJ(stringCodePointAt(string, index));
  }

  if (!IS_RANGE(args[1])) {
    RETURN_ERROR("Subscript must be a number or a range");
  }

  int step;
  uint32_t count = string->length;
  uint32_t start = calculateRange(AS_RANGE(args[1]), &count, &step);
  if (start == UINT32_MAX) return false;

  RETURN_OBJ(stringFromRange(string, start, count, step));
}


DEF_NATIVE(string_indexOf1) {
  if (!validateString(args[1], "Argument")) return false;

  ObjString* string = AS_STRING(args[0]);
  ObjString* search = AS_STRING(args[1]);

  uint32_t index = stringFind(string, search, 0);
  RETURN_NUMBER(index == UINT32_MAX ? -1 : (int)index);
}

DEF_NATIVE(string_indexOf2) {
  if (!validateString(args[1], "Argument")) return false;

  ObjString* string = AS_STRING(args[0]);
  ObjString* search = AS_STRING(args[1]);
  uint32_t start = validateIndex(args[2], string->length, "Start");
  if (start == UINT32_MAX) return false;

  uint32_t index = stringFind(string, search, start);
  RETURN_NUMBER(index == UINT32_MAX ? -1 : (int)index);
}

DEF_NATIVE(string_iterate) {
  ObjString* string = AS_STRING(args[0]);

  if (IS_NONE(args[1])) {
    if (string->length == 0) RETURN_FALSE();
    RETURN_NUMBER(0);
  }

  if (!validateInt(args[1], "Iterator")) return false;

  if (AS_NUMBER(args[1]) < 0) RETURN_FALSE();
  uint32_t index = (uint32_t)AS_NUMBER(args[1]);

  do {
    index++;
    if (index >= string->length) RETURN_FALSE();
  } while ((string->chars[index] & 0xc0) == 0x80);

  RETURN_NUMBER(index);
}

DEF_NATIVE(string_iterateByte) {
  ObjString* string = AS_STRING(args[0]);

  if (IS_NONE(args[1])) {
    if (string->length == 0) RETURN_FALSE();
    RETURN_NUMBER(0);
  }

  if (!validateInt(args[1], "Iterator")) return false;

  if (AS_NUMBER(args[1]) < 0) RETURN_FALSE();
  uint32_t index = (uint32_t)AS_NUMBER(args[1]);

  index++;
  if (index >= string->length) RETURN_FALSE();

  RETURN_NUMBER(index);
}

DEF_NATIVE(string_iteratorValue) {
  ObjString* string = AS_STRING(args[0]);
  uint32_t index = validateIndex(args[1], string->length, "Iterator");
  if (index == UINT32_MAX) return false;

  RETURN_OBJ(stringCodePointAt(string, index));
}

DEF_NATIVE(string_lowercase) {
  char* string = AS_CSTRING(args[0]);
  int length = AS_STRING(args[0])->length;

  if (length == 0) RETURN_VAL(args[0]);

  char* copy = ALLOCATE(char, length);
  strcpy(copy, string);
  for (int i = 0; i < length; i++) {
    copy[i] = tolower(copy[i]);
  }

  RETURN_OBJ(takeString(copy, length));
}

DEF_NATIVE(string_rangeDotDot) {
  if (!validateString(args[1], "Right hand side of range")) return false;
  
  ObjString* from = AS_STRING(args[0]);
  ObjString* to = AS_STRING(args[1]);

  int fromBytes = utf8DecodeNumBytes(from->chars[0]);
  int toBytes = utf8DecodeNumBytes(to->chars[0]);

  if (from->length == 0 || from->length > fromBytes) RETURN_ERROR("Left hand side of range must be a single character");
  if (to->length == 0 || to->length > toBytes) RETURN_ERROR("Right hand side of range must be a single character");

  RETURN_OBJ(newRange(utf8Decode((uint8_t*)from->chars, fromBytes), utf8Decode((uint8_t*)to->chars, toBytes), true));
}

DEF_NATIVE(string_rangeDotDotLess) {
  if (!validateString(args[1], "Right hand side of range")) return false;
  
  ObjString* from = AS_STRING(args[0]);
  ObjString* to = AS_STRING(args[1]);

  int fromBytes = utf8DecodeNumBytes(from->chars[0]);
  int toBytes = utf8DecodeNumBytes(to->chars[0]);

  if (from->length == 0 || from->length > fromBytes) RETURN_ERROR("Left hand side of range must be a single character");
  if (to->length == 0 || to->length > toBytes) RETURN_ERROR("Right hand side of range must be a single character");

  RETURN_OBJ(newRange(utf8Decode((uint8_t*)from->chars, fromBytes), utf8Decode((uint8_t*)to->chars, toBytes), false));
}

DEF_NATIVE(string_startsWith) {
  if (!validateString(args[1], "Argument")) return false;

  ObjString* string = AS_STRING(args[0]);
  ObjString* search = AS_STRING(args[1]);

  if (search->length > string->length) RETURN_FALSE();

  RETURN_BOOL(memcmp(string->chars, search->chars, search->length) == 0);
}

DEF_NATIVE(string_toString) { RETURN_VAL(args[0]); }

//////////////////
// Sys          //
//////////////////

DEF_NATIVE(sys_clock) {
  RETURN_NUMBER((double)clock() / CLOCKS_PER_SEC);
}

DEF_NATIVE(sys_delay) {
  if (!validateInt(args[1], "Time value")) return false;
  uint32_t millis = AS_NUMBER(args[1]);

  struct timespec ts;
  int res;

  if (millis < 0) {
    RETURN_ERROR("Time value cannot be negative");
  }

  ts.tv_sec = millis / 1000;
  ts.tv_nsec = (millis % 1000) * 1000000;

  // do {
  res = nanosleep(&ts, &ts);
  // } while (res && errno == EINTR);

  RETURN_NUMBER(res);
}

DEF_NATIVE(sys_readFile) {
  if (!validateString(args[1], "Filename")) return false;
  const char* filename = AS_CSTRING(args[1]);

  FILE* file = fopen(filename, "rb");
  if (file == NULL) {
    RETURN_ERROR("Couldn't open file '%s'", filename);
  }

  fseek(file, 0L, SEEK_END);
  size_t fileSize = ftell(file);
  rewind(file);

  char* buffer = ALLOCATE(char, fileSize + 1);
  if (buffer == NULL) {
    RETURN_ERROR("Not enough memory to read '%s'", filename);
  }

  size_t bytesRead = fread(buffer, sizeof(char), fileSize, file);
  if (bytesRead < fileSize) {
    RETURN_ERROR("Failed to read '%s'", file);
  }

  buffer[bytesRead] = '\0';
  fclose(file);

  RETURN_OBJ(takeString(buffer, fileSize + 1));
}

DEF_NATIVE(sys_gc) {
  collectGarbage();
  RETURN_NONE();
}

DEF_NATIVE(sys_printStack) {
  printStack(&vm);
  RETURN_NONE();
}

DEF_NATIVE(sys_disassemble) {
  if (!validateFunction(args[1], "Argument")) return false;
  ObjFunction* function = AS_CLOSURE(args[1])->function;
  disassembleChunk(&function->chunk, function->name->chars);
  RETURN_NONE();
}

DEF_NATIVE(sys_input) {
  if (!validateString(args[1], "Prompt")) return false;
  printf("%s", AS_CSTRING(args[1]));

  char* buffer = NULL;
  size_t bufferSize;
  int read = getline(&buffer, &bufferSize, stdin);
  if (read == -1) {
    printf("\n");
    RETURN_ERROR("Cannot read past input EOF");
  }

  buffer[strcspn(buffer, "\r\n")] = '\0';

  RETURN_OBJ(takeString(buffer, (int)strlen(buffer)));
}

DEF_NATIVE(sys_printString) {
  printf("%s\n", AS_CSTRING(args[1]));
  RETURN_VAL(args[1]);
}

DEF_NATIVE(sys_writeString) {
  printf("%s", AS_CSTRING(args[1]));
  RETURN_VAL(args[1]);
}

////////////////////
// Tuple          //
////////////////////

DEF_NATIVE(tuple_fromList) {
  if (!IS_LIST(args[1])) RETURN_ERROR("Argument must be a list");

  ObjList* list = AS_LIST(args[1]);
  uint32_t length = list->count;

  ObjTuple* tuple = newTuple(length);

  for (int i = 0; i < length; i++) {
    tuple->items[i] = list->items[i];
  }

  RETURN_OBJ(tuple);
}

DEF_NATIVE(tuple_blank) { RETURN_OBJ(newTuple(0)); }

#define TUPLE_OF(length)                \
  DEF_NATIVE(tuple_of##length) {        \
    ObjTuple* tuple = newTuple(length); \
    for (int i = 0; i < length; i++) {  \
      tuple->items[i] = args[i + 1];    \
    }                                   \
    RETURN_OBJ(tuple);                  \
  }

TUPLE_OF(1)
TUPLE_OF(2)
TUPLE_OF(3)
TUPLE_OF(4)
TUPLE_OF(5)
TUPLE_OF(6)
TUPLE_OF(7)
TUPLE_OF(8)
TUPLE_OF(9)
TUPLE_OF(10)
TUPLE_OF(11)
TUPLE_OF(12)
TUPLE_OF(13)
TUPLE_OF(14)
TUPLE_OF(15)
TUPLE_OF(16)

DEF_NATIVE(tuple_get) {
  ObjTuple* tuple = AS_TUPLE(args[0]);
  uint32_t index = validateIndex(args[1], tuple->count, "Index");
  if (index == UINT32_MAX) return false;

  RETURN_VAL(tuple->items[index]);
}

DEF_NATIVE(tuple_iterate) {
  ObjTuple* tuple = AS_TUPLE(args[0]);

  if (IS_NONE(args[1])) {
    if (tuple->count == 0) RETURN_FALSE();
    RETURN_NUMBER(0);
  }

  if (!validateInt(args[1], "Iterator")) return false;

  double index = AS_NUMBER(args[1]);
  if (index < 0 || index >= tuple->count - 1) RETURN_FALSE();

  RETURN_NUMBER(index + 1);
}

DEF_NATIVE(tuple_iteratorValue) {
  ObjTuple* tuple = AS_TUPLE(args[0]);
  uint32_t index = validateIndex(args[1], tuple->count, "Iterator");
  if (index == UINT32_MAX) return false;

  RETURN_VAL(tuple->items[index]);
}

DEF_NATIVE(tuple_size) { RETURN_NUMBER(AS_TUPLE(args[0])->count); }

/////////////////////////////
// End of natives          //
/////////////////////////////

static ObjClass* defineClass(VM* vm, ObjModule* module, const char* name) {
  ObjString* className = copyString(name);
  pushRoot((Obj*)className);

  ObjClass* cls = newSingleClass(className);
  tableSet(&module->variables, className, OBJ_VAL(cls), true);

  popRoot();
  return cls;
}

#define GET_CORE_CLASS(cls, name)                                     \
  do {                                                                \
    Value value;                                                      \
    if (tableGet(&coreModule->variables, copyString(name), &value)) { \
      cls = AS_CLASS(value);                                          \
    } else {                                                          \
      ASSERT(false, "Class should already be defined");               \
    }                                                                 \
  } while (false)

void initializeCore(VM* vm) {
  ObjModule* coreModule = newModule(vm->coreString, true);
  pushRoot((Obj*)coreModule);
  tableSet(&vm->modules, vm->coreString, OBJ_VAL(coreModule), true);
  popRoot();

  // You might notice that there are two types of natives that fall under the 'zero arity'
  // category. These are attributes and basic zero-arity methods. This arity is only used
  // when the bound method version (created with ::) of these are called, so to call an
  // attribute, we use foo(). If there is another method called foo(), it won't matter;
  // the process of creating the callable will still determine which one is chosen.

  vm->objectClass = defineClass(vm, coreModule, "Object");
  NATIVE(vm->objectClass, "not()", 0, object_not);
  NATIVE(vm->objectClass, "==(1)", 1, object_equals);
  NATIVE(vm->objectClass, "!=(1)", 1, object_notEquals);
  NATIVE(vm->objectClass, "is(1)", 1, object_is);
  NATIVE(vm->objectClass, "toString()", 0, object_toString);
  NATIVE(vm->objectClass, "type", 0, object_type);

  vm->classClass = defineClass(vm, coreModule, "Class");
  bindSuperclass(vm->classClass, vm->objectClass);
  NATIVE(vm->classClass, "name", 0, class_name);
  NATIVE(vm->classClass, "supertype", 0, class_supertype);
  NATIVE(vm->classClass, "toString()", 0, class_toString);

  ObjClass* objectMetaclass = defineClass(vm, coreModule, "Object metaclass");

  vm->objectClass->obj.cls = objectMetaclass;
  objectMetaclass->obj.cls = vm->classClass;
  vm->classClass->obj.cls = vm->classClass;

  NATIVE(vm->objectClass->obj.cls, "same(2)", 2, object_same);

  InterpretResult coreResult = interpret(coreSource, "core", false);
  if (coreResult != INTERPRET_OK) {
    fprintf(stderr, "Errors found in core file, aborting\n");
    exit(65);
  }

  GET_CORE_CLASS(vm->boolClass, "Bool");
  NATIVE(vm->boolClass, "not()", 0, bool_not);
  NATIVE(vm->boolClass, "toString()", 0, bool_toString);

  GET_CORE_CLASS(vm->boundMethodClass, "BoundMethod");
  NATIVE(vm->boundMethodClass, "arity", 0, boundMethod_arity);
  NATIVE(vm->boundMethodClass, "receiver", 0, boundMethod_receiver);
  NATIVE(vm->boundMethodClass, "toString()", 0, boundMethod_toString);

  GET_CORE_CLASS(vm->noneClass, "None");
  NATIVE(vm->noneClass, "not()", 0, none_not);
  NATIVE(vm->noneClass, "toString()", 0, none_toString);

  GET_CORE_CLASS(vm->functionClass, "Function");
  NATIVE(vm->functionClass, "arity", 0, function_arity);
  NATIVE(vm->functionClass, "toString()", 0, function_toString);

  GET_CORE_CLASS(vm->numberClass, "Number");
  NATIVE(vm->numberClass->obj.cls, "fromString(1)", 1, number_fromString);
  NATIVE(vm->numberClass->obj.cls, "infinity", 0, number_infinity);
  NATIVE(vm->numberClass->obj.cls, "nan", 0, number_nan);
  NATIVE(vm->numberClass->obj.cls, "pi", 0, number_pi);
  NATIVE(vm->numberClass->obj.cls, "tau", 0, number_tau);
  NATIVE(vm->numberClass->obj.cls, "maxDouble", 0, number_maxDouble);
  NATIVE(vm->numberClass->obj.cls, "minDouble", 0, number_minDouble);
  NATIVE(vm->numberClass->obj.cls, "maxInteger", 0, number_maxInteger);
  NATIVE(vm->numberClass->obj.cls, "minInteger", 0, number_minInteger);
  NATIVE(vm->numberClass, "+(1)", 1, number_plus);
  NATIVE(vm->numberClass, "-(1)", 1, number_minus);
  NATIVE(vm->numberClass, "*(1)", 1, number_multiply);
  NATIVE(vm->numberClass, "/(1)", 1, number_divide);
  NATIVE(vm->numberClass, "**(1)", 1, number_pow);
  NATIVE(vm->numberClass, "<(1)", 1, number_lt);
  NATIVE(vm->numberClass, ">(1)", 1, number_gt);
  NATIVE(vm->numberClass, "<=(1)", 1, number_lte);
  NATIVE(vm->numberClass, ">=(1)", 1, number_gte);
  NATIVE(vm->numberClass, "==(1)", 1, number_equals);
  NATIVE(vm->numberClass, "!=(1)", 1, number_notEquals);
  NATIVE(vm->numberClass, "&(1)", 1, number_bitwiseAnd);
  NATIVE(vm->numberClass, "|(1)", 1, number_bitwiseOr);
  NATIVE(vm->numberClass, "^(1)", 1, number_bitwiseXor);
  NATIVE(vm->numberClass, "shl(1)", 1, number_bitwiseLeftShift);
  NATIVE(vm->numberClass, "shr(1)", 1, number_bitwiseRightShift);
  NATIVE(vm->numberClass, "abs()", 0, number_abs);
  NATIVE(vm->numberClass, "acos()", 0, number_acos);
  NATIVE(vm->numberClass, "asin()", 0, number_asin);
  NATIVE(vm->numberClass, "atan()", 0, number_atan);
  NATIVE(vm->numberClass, "cbrt()", 0, number_cbrt);
  NATIVE(vm->numberClass, "ceil()", 0, number_ceil);
  NATIVE(vm->numberClass, "cos()", 0, number_cos);
  NATIVE(vm->numberClass, "floor()", 0, number_floor);
  NATIVE(vm->numberClass, "-()", 0, number_negate);
  NATIVE(vm->numberClass, "round()", 0, number_round);
  NATIVE(vm->numberClass, "min(1)", 1, number_min);
  NATIVE(vm->numberClass, "max(1)", 1, number_max);
  NATIVE(vm->numberClass, "clamp(2)", 2, number_clamp);
  NATIVE(vm->numberClass, "gcf(1)", 1, number_gcd);
  NATIVE(vm->numberClass, "gcd(1)", 1, number_gcd);
  NATIVE(vm->numberClass, "lcm(1)", 1, number_lcm);
  NATIVE(vm->numberClass, "sin()", 0, number_sin);
  NATIVE(vm->numberClass, "sqrt()", 0, number_sqrt);
  NATIVE(vm->numberClass, "tan()", 0, number_tan);
  NATIVE(vm->numberClass, "log()", 0, number_log);
  NATIVE(vm->numberClass, "log2()", 0, number_log2);
  NATIVE(vm->numberClass, "exp()", 0, number_exp);
  NATIVE(vm->numberClass, "%(1)", 1, number_mod);
  NATIVE(vm->numberClass, "~()", 0, number_bitwiseNot);
  NATIVE(vm->numberClass, "..(1)", 1, number_rangeDotDot);
  NATIVE(vm->numberClass, "..<(1)", 1, number_rangeDotDotLess);
  NATIVE(vm->numberClass, "atan(1)", 1, number_atan2);
  NATIVE(vm->numberClass, "fraction()", 0, number_fraction);
  NATIVE(vm->numberClass, "isInfinity", 0, number_isInfinity);
  NATIVE(vm->numberClass, "isInteger", 0, number_isInteger);
  NATIVE(vm->numberClass, "isNan", 0, number_isNan);
  NATIVE(vm->numberClass, "sign", 0, number_sign);
  NATIVE(vm->numberClass, "toString()", 0, number_toString);
  NATIVE(vm->numberClass, "truncate()", 0, number_truncate);

  GET_CORE_CLASS(vm->randomClass, "Random");
  NATIVE_INIT(vm->randomClass, random_init, 0);
  NATIVE(vm->randomClass->obj.cls, "seed(1)", 1, random_seed);
  NATIVE(vm->randomClass, "randBytes(1)", 1, random_randBytes);

  GET_CORE_CLASS(vm->stringClass, "String");
  NATIVE(vm->stringClass->obj.cls, "fromCodePoint(1)", 1, string_fromCodePoint);
  NATIVE(vm->stringClass->obj.cls, "fromByte(1)", 1, string_fromByte);
  NATIVE(vm->stringClass, "byteAt(1)", 1, string_byteAt);
  NATIVE(vm->stringClass, "byteCount", 0, string_byteCount);
  NATIVE(vm->stringClass, "length", 0, string_byteCount);
  NATIVE(vm->stringClass, "codePointAt(1)", 1, string_codePointAt);
  NATIVE(vm->stringClass, "concatenate(1)", 1, string_concatenate);
  NATIVE(vm->stringClass, "contains(1)", 1, string_contains);
  NATIVE(vm->stringClass, "endsWith(1)", 1, string_endsWith);
  NATIVE(vm->stringClass, "get(1)", 1, string_get);
  NATIVE(vm->stringClass, "indexOf(1)", 1, string_indexOf1);
  NATIVE(vm->stringClass, "indexOf(2)", 2, string_indexOf2);
  NATIVE(vm->stringClass, "iterate(1)", 1, string_iterate);
  NATIVE(vm->stringClass, "iterateByte(1)", 1, string_iterateByte);
  NATIVE(vm->stringClass, "iteratorValue(1)", 1, string_iteratorValue);
  NATIVE(vm->stringClass, "lowercase()", 0, string_lowercase);
  NATIVE(vm->stringClass, "..(1)", 1, string_rangeDotDot);
  NATIVE(vm->stringClass, "..<(1)", 1, string_rangeDotDotLess);
  NATIVE(vm->stringClass, "startsWith(1)", 1, string_startsWith);
  NATIVE(vm->stringClass, "toString()", 0, string_toString);

  GET_CORE_CLASS(vm->listClass, "List");
  NATIVE_INIT(vm->listClass, list_init, 0);
  NATIVE(vm->listClass->obj.cls, "filled(2)", 2, list_filled);
  NATIVE(vm->listClass, "get(1)", 1, list_get);
  NATIVE(vm->listClass, "set(2)", 2, list_set);
  NATIVE(vm->listClass, "add(1)", 1, list_add);
  NATIVE(vm->listClass, "addCore(1)", 1, list_addCore);
  NATIVE(vm->listClass, "clear()", 0, list_clear);
  NATIVE(vm->listClass, "indexOf(1)", 1, list_indexOf);
  NATIVE(vm->listClass, "insert(2)", 2, list_insert);
  NATIVE(vm->listClass, "iterate(1)", 1, list_iterate);
  NATIVE(vm->listClass, "iteratorValue(1)", 1, list_iteratorValue);
  NATIVE(vm->listClass, "removeAt(1)", 1, list_removeAt);
  NATIVE(vm->listClass, "remove(1)", 1, list_removeValue);
  NATIVE(vm->listClass, "size", 0, list_size);
  NATIVE(vm->listClass, "count", 0, list_size);
  NATIVE(vm->listClass, "swap(2)", 2, list_swap);

  GET_CORE_CLASS(vm->mapClass, "Map");
  NATIVE_INIT(vm->mapClass, map_init, 0);
  NATIVE(vm->mapClass, "get(1)", 1, map_get);
  NATIVE(vm->mapClass, "set(2)", 2, map_set);
  NATIVE(vm->mapClass, "addCore(2)", 2, map_addCore);
  NATIVE(vm->mapClass, "clear()", 0, map_clear);
  NATIVE(vm->mapClass, "containsKey(1)", 1, map_containsKey);
  NATIVE(vm->mapClass, "remove(1)", 1, map_remove);
  NATIVE(vm->mapClass, "size", 0, map_size);
  NATIVE(vm->mapClass, "count", 0, map_size);
  NATIVE(vm->mapClass, "iterate(1)", 1, map_iterate);
  NATIVE(vm->mapClass, "keyIteratorValue(1)", 1, map_keyIteratorValue);
  NATIVE(vm->mapClass, "valueIteratorValue(1)", 1, map_valueIteratorValue);

  GET_CORE_CLASS(vm->rangeClass, "Range");
  NATIVE_INIT(vm->rangeClass, range_init, 3);
  NATIVE(vm->rangeClass, "from", 0, range_from);
  NATIVE(vm->rangeClass, "to", 0, range_to);
  NATIVE(vm->rangeClass, "min", 0, range_min);
  NATIVE(vm->rangeClass, "max", 0, range_max);
  NATIVE(vm->rangeClass, "isInclusive", 0, range_isInclusive);
  NATIVE(vm->rangeClass, "contains(1)", 1, range_contains);
  NATIVE(vm->rangeClass, "includes(1)", 1, range_includes);
  NATIVE(vm->rangeClass, "iterate(1)", 1, range_iterate);
  NATIVE(vm->rangeClass, "iteratorValue(1)", 1, range_iteratorValue);
  NATIVE(vm->rangeClass, "toString()", 0, range_toString);

  GET_CORE_CLASS(vm->tupleClass, "Tuple");
  NATIVE(vm->tupleClass->obj.cls, "fromList(1)", 1, tuple_fromList);
  NATIVE(vm->tupleClass->obj.cls, "blank()", 0, tuple_blank);
  NATIVE(vm->tupleClass->obj.cls, "of(1)", 1, tuple_of1);
  NATIVE(vm->tupleClass->obj.cls, "of(2)", 2, tuple_of2);
  NATIVE(vm->tupleClass->obj.cls, "of(3)", 3, tuple_of3);
  NATIVE(vm->tupleClass->obj.cls, "of(4)", 4, tuple_of4);
  NATIVE(vm->tupleClass->obj.cls, "of(5)", 5, tuple_of5);
  NATIVE(vm->tupleClass->obj.cls, "of(6)", 6, tuple_of6);
  NATIVE(vm->tupleClass->obj.cls, "of(7)", 7, tuple_of7);
  NATIVE(vm->tupleClass->obj.cls, "of(8)", 8, tuple_of8);
  NATIVE(vm->tupleClass->obj.cls, "of(9)", 9, tuple_of9);
  NATIVE(vm->tupleClass->obj.cls, "of(10)", 10, tuple_of10);
  NATIVE(vm->tupleClass->obj.cls, "of(11)", 11, tuple_of11);
  NATIVE(vm->tupleClass->obj.cls, "of(12)", 12, tuple_of12);
  NATIVE(vm->tupleClass->obj.cls, "of(13)", 13, tuple_of13);
  NATIVE(vm->tupleClass->obj.cls, "of(14)", 14, tuple_of14);
  NATIVE(vm->tupleClass->obj.cls, "of(15)", 15, tuple_of15);
  NATIVE(vm->tupleClass->obj.cls, "of(16)", 16, tuple_of16);
  NATIVE(vm->tupleClass, "get(1)", 1, tuple_get);
  NATIVE(vm->tupleClass, "iterate(1)", 1, tuple_iterate);
  NATIVE(vm->tupleClass, "iteratorValue(1)", 1, tuple_iteratorValue);
  NATIVE(vm->tupleClass, "size", 0, tuple_size);
  NATIVE(vm->tupleClass, "count", 0, tuple_size);

  ObjClass* sysClass;
  GET_CORE_CLASS(sysClass, "Sys");
  NATIVE(sysClass->obj.cls, "clock", 0, sys_clock);
  NATIVE(sysClass->obj.cls, "delay(1)", 1, sys_delay);
  NATIVE(sysClass->obj.cls, "readFile(1)", 1, sys_readFile);
  NATIVE(sysClass->obj.cls, "gc()", 0, sys_gc);
  NATIVE(sysClass->obj.cls, "printStack()", 0, sys_printStack);
  NATIVE(sysClass->obj.cls, "disassemble(1)", 1, sys_disassemble);
  NATIVE(sysClass->obj.cls, "input(1)", 1, sys_input);
  NATIVE(sysClass->obj.cls, "printString(1)", 1, sys_printString);
  NATIVE(sysClass->obj.cls, "writeString(1)", 1, sys_writeString);

  // Some string objects were created before stringClass even existed. Those
  // strings have a NULL classObj, so that needs to be fixed.
  for (Obj* obj = vm->objects; obj != NULL; obj = obj->next) {
    if (obj->type == OBJ_STRING) obj->cls = vm->stringClass;
  }
}
