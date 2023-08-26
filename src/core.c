#include "core.h"

#include <ctype.h>
#include <errno.h>
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "memory.h"
#include "object.h"
#include "primitive.h"
#include "utils.h"
#include "value.h"

///////////////////
// Bool          //
///////////////////

DEF_PRIMITIVE(bool_not) { RETURN_BOOL(!AS_BOOL(args[0])); }

DEF_PRIMITIVE(bool_toString) {
  if (AS_BOOL(args[0])) {
    RETURN_OBJ(copyStringLength("True", 4));
  } else {
    RETURN_OBJ(copyStringLength("False", 5));
  }
}

////////////////////
// Class          //
////////////////////

DEF_PRIMITIVE(class_name) { RETURN_OBJ(AS_CLASS(args[0])->name); }

DEF_PRIMITIVE(class_superclass) {
  ObjClass* cls = AS_CLASS(args[0]);

  if (cls->superclass == NULL) RETURN_NONE;

  RETURN_OBJ(cls->superclass);
}

DEF_PRIMITIVE(class_toString) { RETURN_OBJ(AS_CLASS(args[0])->name); }

///////////////////
// List          //
///////////////////

DEF_PRIMITIVE(list_filled) {
  if (!validateInt(args[1], "Size")) return false;
  if (AS_NUMBER(args[1]) < 0) RETURN_ERROR("Size cannot be negative");

  uint32_t size = (uint32_t)AS_NUMBER(args[1]);
  ObjList* list = newList(size);

  for (uint32_t i = 0; i < size; i++) {
    list->items[i] = args[2];
  }

  RETURN_OBJ(list);
}

DEF_PRIMITIVE(list_new) { RETURN_OBJ(newList(0)); }

DEF_PRIMITIVE(list_add) {
  listAppend(AS_LIST(args[0]), args[1]);
  RETURN_NONE;
}

DEF_PRIMITIVE(list_addCore) {
  listAppend(AS_LIST(args[0]), args[1]);
  RETURN_VAL(args[0]);
}

DEF_PRIMITIVE(list_clear) {
  listClear(AS_LIST(args[0]));
  RETURN_NONE;
}

DEF_PRIMITIVE(list_size) { RETURN_NUMBER(AS_LIST(args[0])->count); }

DEF_PRIMITIVE(list_insert) {
  ObjList* list = AS_LIST(args[0]);

  uint32_t index = validateIndex(args[1], list->count + 1, "Index");
  if (index == UINT32_MAX) return false;

  listInsertAt(list, index, args[2]);
  RETURN_VAL(args[2]);
}

DEF_PRIMITIVE(list_iterate) {
  ObjList* list = AS_LIST(args[0]);

  if (IS_NONE(args[1])) {
    if (list->count == 0) RETURN_FALSE;
    RETURN_NUMBER(0);
  }

  if (!validateInt(args[1], "Iterator")) return false;

  double index = AS_NUMBER(args[1]);
  if (index < 0 || index >= list->count - 1) RETURN_FALSE;

  RETURN_NUMBER(index + 1);
}

DEF_PRIMITIVE(list_iteratorValue) {
  ObjList* list = AS_LIST(args[0]);
  uint32_t index = validateIndex(args[1], list->count, "Iterator");
  if (index == UINT32_MAX) return false;

  RETURN_VAL(list->items[index]);
}

DEF_PRIMITIVE(list_removeAt) {
  ObjList* list = AS_LIST(args[0]);
  uint32_t index = validateIndex(args[1], list->count, "Index");
  if (index == UINT32_MAX) return false;

  RETURN_VAL(listDeleteAt(list, index));
}

DEF_PRIMITIVE(list_removeValue) {
  ObjList* list = AS_LIST(args[0]);
  int index = listIndexOf(list, args[1]);
  if (index == -1) RETURN_NONE;
  RETURN_VAL(listDeleteAt(list, index));
}

DEF_PRIMITIVE(list_indexOf) {
  ObjList* list = AS_LIST(args[0]);
  RETURN_NUMBER(listIndexOf(list, args[1]));
}

DEF_PRIMITIVE(list_swap) {
  ObjList* list = AS_LIST(args[0]);
  uint32_t indexA = validateIndex(args[1], list->count, "Index 0");
  if (indexA == UINT32_MAX) return false;
  uint32_t indexB = validateIndex(args[1], list->count, "Index 1");
  if (indexB == UINT32_MAX) return false;

  Value a = list->items[indexA];
  list->items[indexA] = list->items[indexB];
  list->items[indexB] = a;

  RETURN_NONE;
}

DEF_PRIMITIVE(list_subscript) {
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

DEF_PRIMITIVE(list_subscriptSet) {
  ObjList* list = AS_LIST(args[0]);
  uint32_t index = validateIndex(args[1], list->count, "Index");
  if (index == UINT32_MAX) return false;

  list->items[index] = args[2];
  RETURN_VAL(args[2]);
}

///////////////////
// None          //
///////////////////

DEF_PRIMITIVE(none_not) { RETURN_TRUE; }

DEF_PRIMITIVE(none_toString) { RETURN_OBJ(copyStringLength("None", 4)); }

/////////////////////
// Number          //
/////////////////////

DEF_PRIMITIVE(number_fromString) {
  if (!validateString(args[1], "Argument")) return false;

  ObjString* string = AS_STRING(args[1]);

  if (string->length == 0) RETURN_NONE;

  errno = 0;
  char* end;
  double number = strtod(string->chars, &end);

  while (*end != '\0' && isspace((unsigned char)*end)) end++;

  if (errno = ERANGE) RETURN_ERROR("Number literal is too large");

  if (end < string->chars + string->length) RETURN_NONE;

  RETURN_NUMBER(number);
}

#define DEF_NUM_CONSTANT(name, value) DEF_PRIMITIVE(number_##name) { RETURN_NUMBER(value); }

DEF_NUM_CONSTANT(infinity,   INFINITY)
DEF_NUM_CONSTANT(nan,        DOUBLE_NAN)
DEF_NUM_CONSTANT(pi,         3.141592653589793238462643383279502884197L)
DEF_NUM_CONSTANT(tau,        6.283185307179586476925286766559005768394L)
DEF_NUM_CONSTANT(maxDouble,  DBL_MAX)
DEF_NUM_CONSTANT(minDouble,  DBL_MIN)
DEF_NUM_CONSTANT(maxInteger, 9007199254740991.0)
DEF_NUM_CONSTANT(minInteger, -9007199254740991.0)

#define DEF_NUM_INFIX(name, op, type)                            \
  DEF_PRIMITIVE(number_##name) {                                 \
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
  DEF_PRIMITIVE(number_bitwise##name) {                          \
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
  DEF_PRIMITIVE(number_##name) { RETURN_NUMBER(fn(AS_NUMBER(args[0]))); }

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

DEF_PRIMITIVE(number_mod) {
  if (!validateNumber(args[1], "Right operand")) return false;
  double a = AS_NUMBER(args[0]);
  double b = AS_NUMBER(args[1]);
  double c = fmod(a, b);
  if ((c < 0 && b >= 0) || (b < 0 && a >= 0)) c += b;
  RETURN_NUMBER(c);
}

DEF_PRIMITIVE(number_equals) {
  if (!IS_NUMBER(args[1])) RETURN_FALSE;
  RETURN_BOOL(AS_NUMBER(args[0]) == AS_NUMBER(args[1]));
}

DEF_PRIMITIVE(number_notEquals) {
  if (!IS_NUMBER(args[1])) RETURN_TRUE;
  RETURN_BOOL(AS_NUMBER(args[0]) != AS_NUMBER(args[1]));
}

DEF_PRIMITIVE(number_bitwiseNot) {
  RETURN_NUMBER(!(uint32_t)AS_NUMBER(args[0]));
}

DEF_PRIMITIVE(number_dotDot) {
  if (!validateNumber(args[1], "Right hand side of range")) return false;

  double from = AS_NUMBER(args[0]);
  double to = AS_NUMBER(args[1]);
  RETURN_OBJ(newRange(from, to, true));
}

DEF_PRIMITIVE(number_colon) {
  if (!validateNumber(args[1], "Right hand side of range")) return false;

  double from = AS_NUMBER(args[0]);
  double to = AS_NUMBER(args[1]);
  RETURN_OBJ(newRange(from, to, false));
}

DEF_PRIMITIVE(number_atan2) {
  if (!validateNumber(args[1], "x value")) return false;
  RETURN_NUMBER(atan2(AS_NUMBER(args[0]), AS_NUMBER(args[1])));
}

DEF_PRIMITIVE(number_min) {
  if (!validateNumber(args[1], "Other value")) return false;

  double a = AS_NUMBER(args[0]);
  double b = AS_NUMBER(args[1]);
  RETURN_NUMBER(a <= b ? a : b);
}

DEF_PRIMITIVE(number_max) {
  if (!validateNumber(args[1], "Other value")) return false;

  double a = AS_NUMBER(args[0]);
  double b = AS_NUMBER(args[1]);
  RETURN_NUMBER(a >= b ? a : b);
}

DEF_PRIMITIVE(number_clamp) {
  if (!validateNumber(args[1], "Min value")) return false;
  if (!validateNumber(args[1], "Max value")) return false;

  double value = AS_NUMBER(args[0]);
  double min = AS_NUMBER(args[1]);
  double max = AS_NUMBER(args[2]);
  RETURN_NUMBER((value < min) ? min : ((value > max) ? max : value));
}

DEF_PRIMITIVE(number_pow) {
  if (!validateNumber(args[1], "Power value")) return false;
  RETURN_NUMBER(pow(AS_NUMBER(args[0]), AS_NUMBER(args[1])));
}

DEF_PRIMITIVE(number_fraction) {
  double unused;
  RETURN_NUMBER(modf(AS_NUMBER(args[0]), &unused));
}

DEF_PRIMITIVE(number_isInfinity) {
  RETURN_BOOL(isinf(AS_NUMBER(args[0])));
}

DEF_PRIMITIVE(number_isInteger) {
  double value = AS_NUMBER(args[0]);
  if (isnan(value) || isinf(value)) RETURN_FALSE;
  RETURN_BOOL(trunc(value) == value);
}

DEF_PRIMITIVE(number_isNan) {
  RETURN_BOOL(isnan(AS_NUMBER(args[0])));
}

DEF_PRIMITIVE(number_sign) {
  double value = AS_NUMBER(args[0]);
  if (value > 0)
    RETURN_NUMBER(1);
  else if (value < 0)
    RETURN_NUMBER(-1);
  else
    RETURN_NUMBER(0);
}

DEF_PRIMITIVE(number_toString) {
  RETURN_OBJ(numberToString(AS_NUMBER(args[0])));
}

DEF_PRIMITIVE(number_truncate) {
  double integer;
  modf(AS_NUMBER(args[0]), &integer);
  RETURN_NUMBER(integer);
}

/////////////////////
// Object          //
/////////////////////

DEF_PRIMITIVE(object_same) {
  RETURN_BOOL(valuesEqual(args[1], args[2]));
}

DEF_PRIMITIVE(object_not) { RETURN_FALSE; }

DEF_PRIMITIVE(object_equals) {
  RETURN_BOOL(valuesEqual(args[0], args[1]));
}

DEF_PRIMITIVE(object_notEquals) {
  RETURN_BOOL(!valuesEqual(args[0], args[1]));
}

DEF_PRIMITIVE(object_is) {
  if (!IS_CLASS(args[1])) {
    // ERROR("Right operand must be a class");
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

DEF_PRIMITIVE(object_toString) {
  Obj* obj = AS_OBJ(args[0]);
  ObjString* name = obj->cls->name;
  RETURN_OBJ(stringFormat("# instance", name));
}

DEF_PRIMITIVE(object_type) { RETURN_OBJ(getClass(args[0])); }

////////////////////
// Range          //
////////////////////

DEF_PRIMITIVE(range_from) { RETURN_NUMBER(AS_RANGE(args[0])->from); }

DEF_PRIMITIVE(range_to) { RETURN_NUMBER(AS_RANGE(args[0])->to); }

DEF_PRIMITIVE(range_min) {
  ObjRange* range = AS_RANGE(args[0]);
  RETURN_NUMBER(fmin(range->from, range->to));
}

DEF_PRIMITIVE(range_max) {
  ObjRange* range = AS_RANGE(args[0]);
  RETURN_NUMBER(fmax(range->from, range->to));
}

DEF_PRIMITIVE(range_isInclusive) {
  RETURN_BOOL(AS_RANGE(args[0])->isInclusive);
}

DEF_PRIMITIVE(range_iterate) {
  ObjRange* range = AS_RANGE(args[0]);

  if (range->from == range->to && !range->isInclusive) RETURN_FALSE;

  if (IS_NONE(args[1])) RETURN_NUMBER(range->from);

  if (!validateNumber(args[1], "Iterator")) return false;
  double iterator = AS_NUMBER(args[1]);

  if (range->from < range->to) {
    iterator++;
    if (iterator > range->to) RETURN_FALSE;
  } else {
    iterator--;
    if (iterator < range->to) RETURN_FALSE;
  }

  if (!range->isInclusive && iterator == range->to) RETURN_FALSE;

  RETURN_NUMBER(iterator);
}

DEF_PRIMITIVE(range_iteratorValue) { RETURN_VAL(args[1]); }

DEF_PRIMITIVE(range_toString) {
  ObjRange* range = AS_RANGE(args[0]);

  ObjString* from = numberToString(range->from);
  pushRoot((Obj*)from);
  ObjString* to = numberToString(range->to);
  pushRoot((Obj*)to);

  ObjString* result = stringFormat("#$#", from, ":\0.." + 2 * range->isInclusive, to);

  popRoot();
  popRoot();
  RETURN_OBJ(result);
}

/////////////////////
// String          //
/////////////////////

DEF_PRIMITIVE(string_fromCodePoint) {
  if (!validateInt(args[1], "Code point")) return false;
  int codePoint = (int)AS_NUMBER(args[1]);

  if (codePoint < 0) {
    RETURN_ERROR("Code point cannot be negative");
  } else if (codePoint > 0x10ffff) {
    RETURN_ERROR("Code point cannot be greater than 0x10ffff");
  }

  RETURN_OBJ(stringFromCodePoint(codePoint));
}

DEF_PRIMITIVE(string_fromByte) {
  if (!validateInt(args[1], "Byte")) return false;
  int byte = (int)AS_NUMBER(args[1]);

  if (byte < 0) {
    RETURN_ERROR("Byte cannot be negative");
  } else if (byte > 0xff) {
    RETURN_ERROR("Byte cannot be greater than 0xff");
  }

  RETURN_OBJ(stringFromByte((uint8_t)byte));
}

DEF_PRIMITIVE(string_byteAt) {
  ObjString* string = AS_STRING(args[0]);

  uint32_t index = validateIndex(args[1], string->length, "Index");
  if (index == UINT32_MAX) return false;

  RETURN_NUMBER((uint8_t)string->chars[index]);
}

DEF_PRIMITIVE(string_byteCount) {
  RETURN_NUMBER(AS_STRING(args[0])->length);
}

DEF_PRIMITIVE(string_codePointAt) {
  ObjString* string = AS_STRING(args[0]);

  uint32_t index = validateIndex(args[1], string->length, "Index");
  if (index == UINT32_MAX) return false;

  const uint8_t* bytes = (uint8_t*)string->chars;
  if ((bytes[index] & 0xc0) == 0x80) RETURN_NUMBER(-1);

  RETURN_NUMBER(utf8Decode((uint8_t*)string->chars + index, string->length - index));
}

DEF_PRIMITIVE(string_contains) {
  if (!validateString(args[1], "Argument")) return false;

  ObjString* string = AS_STRING(args[0]);
  ObjString* search = AS_STRING(args[1]);

  RETURN_BOOL(stringFind(string, search, 0) != UINT32_MAX);
}

DEF_PRIMITIVE(string_endsWith) {
  if (!validateString(args[1], "Argument")) return false;

  ObjString* string = AS_STRING(args[0]);
  ObjString* search = AS_STRING(args[1]);

  if (search->length > string->length) RETURN_FALSE;

  RETURN_BOOL(memcmp(string->chars + string->length - search->length,
                     search->chars, search->length) == 0);
}

DEF_PRIMITIVE(string_indexOf1) {
  if (!validateString(args[1], "Argument")) return false;

  ObjString* string = AS_STRING(args[0]);
  ObjString* search = AS_STRING(args[1]);

  uint32_t index = stringFind(string, search, 0);
  RETURN_NUMBER(index == UINT32_MAX ? -1 : (int)index);
}

DEF_PRIMITIVE(string_indexOf2) {
  if (!validateString(args[1], "Argument")) return false;

  ObjString* string = AS_STRING(args[0]);
  ObjString* search = AS_STRING(args[1]);
  uint32_t start = validateIndex(args[2], string->length, "Start");
  if (start == UINT32_MAX) return false;

  uint32_t index = stringFind(string, search, start);
  RETURN_NUMBER(index == UINT32_MAX ? -1 : (int)index);
}

DEF_PRIMITIVE(string_iterate) {
  ObjString* string = AS_STRING(args[0]);

  if (IS_NONE(args[1])) {
    if (string->length == 0) RETURN_FALSE;
    RETURN_NUMBER(0);
  }

  if (!validateInt(args[1], "Iterator")) return false;

  if (AS_NUMBER(args[1]) < 0) RETURN_FALSE;
  uint32_t index = (uint32_t)AS_NUMBER(args[1]);

  do {
    index++;
    if (index >= string->length) RETURN_FALSE;
  } while ((string->chars[index] & 0xc0) == 0x80);

  RETURN_NUMBER(index);
}

DEF_PRIMITIVE(string_iterateByte) {
  ObjString* string = AS_STRING(args[0]);

  if (IS_NONE(args[1])) {
    if (string->length == 0) RETURN_FALSE;
    RETURN_NUMBER(0);
  }

  if (!validateInt(args[1], "Iterator")) return false;

  if (AS_NUMBER(args[1]) < 0) RETURN_FALSE;
  uint32_t index = (uint32_t)AS_NUMBER(args[1]);

  index++;
  if (index >= string->length) RETURN_FALSE;

  RETURN_NUMBER(index);
}

DEF_PRIMITIVE(string_iteratorValue) {
  ObjString* string = AS_STRING(args[0]);
  uint32_t index = validateIndex(args[1], string->length, "Iterator");
  if (index == UINT32_MAX) return false;

  RETURN_OBJ(stringCodePointAt(string, index));
}

DEF_PRIMITIVE(string_startsWith) {
  if (!validateString(args[1], "Argument")) return false;

  ObjString* string = AS_STRING(args[0]);
  ObjString* search = AS_STRING(args[1]);

  if (search->length > string->length) RETURN_FALSE;

  RETURN_BOOL(memcmp(string->chars, search->chars, search->length) == 0);
}

DEF_PRIMITIVE(string_plus) {
  if (!validateString(args[1], "Right operand")) return false;
  RETURN_OBJ(stringFormat("##", args[0], args[1]));
}

DEF_PRIMITIVE(string_subscript) {
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

DEF_PRIMITIVE(string_toString) { RETURN_VAL(args[0]); }

//////////////////
// Sys          //
//////////////////

DEF_PRIMITIVE(sys_clock) {
  RETURN_NUMBER((double)clock() / CLOCKS_PER_SEC);
}

DEF_PRIMITIVE(sys_gc) {
  collectGarbage();
  RETURN_NONE;
}

DEF_PRIMITIVE(sys_writeString) {
  printf("%s\n", AS_CSTRING(args[1]));
  RETURN_VAL(args[1]);
}

////////////////////////////////
// End of primitives          //
////////////////////////////////

static ObjClass* defineClass(VM* vm, const char* name) {
  ObjString* className = copyString(name);
  pushRoot((Obj*)className);

  ObjClass* cls = newClass(className);
  tableSet(&vm->globals, className, OBJ_VAL(cls));

  popRoot();
  return cls;
}

#define GET_CORE_CLASS(cls, name)                           \
  do {                                                      \
    Value value;                                            \
    if (tableGet(&vm->globals, copyString(name), &value)) { \
      cls = AS_CLASS(value);                                \
    }                                                       \
  } while (false)

void initializeCore(VM* vm) {
  vm->objectClass = defineClass(vm, "Object");
  PRIM_STATIC(vm->objectClass, "same(2)", object_same);
  PRIMITIVE(vm->objectClass, "not", object_not);
  PRIMITIVE(vm->objectClass, "==(1)", object_equals);
  PRIMITIVE(vm->objectClass, "!=(1)", object_notEquals);
  PRIMITIVE(vm->objectClass, "is(1)", object_is);
  PRIMITIVE(vm->objectClass, "toString", object_toString);
  PRIMITIVE(vm->objectClass, "type", object_type);

  vm->classClass = defineClass(vm, "Class");
  bindSuperclass(vm->classClass, vm->objectClass);
  PRIMITIVE(vm->classClass, "name", class_name);
  PRIMITIVE(vm->classClass, "superclass", class_superclass);
  PRIMITIVE(vm->classClass, "toString", class_toString);

  // TODO: Interpret a source file with the rest of the standard library

  GET_CORE_CLASS(vm->boolClass, "Bool");
  PRIMITIVE(vm->boolClass, "toString", bool_toString);
  PRIMITIVE(vm->boolClass, "not", bool_not);

  GET_CORE_CLASS(vm->noneClass, "None");
  PRIMITIVE(vm->noneClass, "not", none_not);
  PRIMITIVE(vm->noneClass, "toString", none_toString);

  GET_CORE_CLASS(vm->numberClass, "Number");
  PRIM_STATIC(vm->numberClass, "fromString(1)", number_fromString);
  PRIM_STATIC(vm->numberClass, "infinity", number_infinity);
  PRIM_STATIC(vm->numberClass, "nan", number_nan);
  PRIM_STATIC(vm->numberClass, "pi", number_pi);
  PRIM_STATIC(vm->numberClass, "tau", number_tau);
  PRIM_STATIC(vm->numberClass, "maxDouble", number_maxDouble);
  PRIM_STATIC(vm->numberClass, "minDouble", number_minDouble);
  PRIM_STATIC(vm->numberClass, "maxInteger", number_maxInteger);
  PRIM_STATIC(vm->numberClass, "minInteger", number_minInteger);
  PRIMITIVE(vm->numberClass, "+(1)", number_plus);
  PRIMITIVE(vm->numberClass, "-(1)", number_minus);
  PRIMITIVE(vm->numberClass, "*(1)", number_multiply);
  PRIMITIVE(vm->numberClass, "/(1)", number_divide);
  PRIMITIVE(vm->numberClass, "<(1)", number_lt);
  PRIMITIVE(vm->numberClass, ">(1)", number_gt);
  PRIMITIVE(vm->numberClass, "<=(1)", number_lte);
  PRIMITIVE(vm->numberClass, ">=(1)", number_gte);
  PRIMITIVE(vm->numberClass, "==(1)", number_equals);
  PRIMITIVE(vm->numberClass, "!=(1)", number_notEquals);
  PRIMITIVE(vm->numberClass, "&(1)", number_bitwiseAnd);
  PRIMITIVE(vm->numberClass, "|(1)", number_bitwiseOr);
  PRIMITIVE(vm->numberClass, "^(1)", number_bitwiseXor);
  PRIMITIVE(vm->numberClass, "shl(1)", number_bitwiseLeftShift);
  PRIMITIVE(vm->numberClass, "shr(1)", number_bitwiseRightShift);
  PRIMITIVE(vm->numberClass, "abs", number_abs);
  PRIMITIVE(vm->numberClass, "acos", number_acos);
  PRIMITIVE(vm->numberClass, "asin", number_asin);
  PRIMITIVE(vm->numberClass, "atan", number_atan);
  PRIMITIVE(vm->numberClass, "cbrt", number_cbrt);
  PRIMITIVE(vm->numberClass, "ceil", number_ceil);
  PRIMITIVE(vm->numberClass, "cos", number_cos);
  PRIMITIVE(vm->numberClass, "floor", number_floor);
  PRIMITIVE(vm->numberClass, "-", number_negate);
  PRIMITIVE(vm->numberClass, "round", number_round);
  PRIMITIVE(vm->numberClass, "min(1)", number_min);
  PRIMITIVE(vm->numberClass, "max(1)", number_max);
  PRIMITIVE(vm->numberClass, "clamp(2)", number_clamp);
  PRIMITIVE(vm->numberClass, "sin", number_sin);
  PRIMITIVE(vm->numberClass, "sqrt", number_sqrt);
  PRIMITIVE(vm->numberClass, "tan", number_tan);
  PRIMITIVE(vm->numberClass, "log", number_log);
  PRIMITIVE(vm->numberClass, "log2", number_log2);
  PRIMITIVE(vm->numberClass, "exp", number_exp);
  PRIMITIVE(vm->numberClass, "%(1)", number_mod);
  PRIMITIVE(vm->numberClass, "~", number_bitwiseNot);
  PRIMITIVE(vm->numberClass, "..(1)", number_dotDot);
  PRIMITIVE(vm->numberClass, ":(1)", number_colon);
  PRIMITIVE(vm->numberClass, "atan(1)", number_atan2);
  PRIMITIVE(vm->numberClass, "pow(1)", number_pow);
  PRIMITIVE(vm->numberClass, "fraction", number_fraction);
  PRIMITIVE(vm->numberClass, "isInfinity", number_isInfinity);
  PRIMITIVE(vm->numberClass, "isInteger", number_isInteger);
  PRIMITIVE(vm->numberClass, "isNan", number_isNan);
  PRIMITIVE(vm->numberClass, "sign", number_sign);
  PRIMITIVE(vm->numberClass, "toString", number_toString);
  PRIMITIVE(vm->numberClass, "truncate", number_truncate);

  GET_CORE_CLASS(vm->stringClass, "String");
  PRIM_STATIC(vm->stringClass, "fromCodePoint(1)", string_fromCodePoint);
  PRIM_STATIC(vm->stringClass, "fromByte(1)", string_fromByte);
  PRIMITIVE(vm->stringClass, "+(1)", string_plus);
  PRIMITIVE(vm->stringClass, "get(1)", string_subscript);
  PRIMITIVE(vm->stringClass, "byteAt(1)", string_byteAt);
  PRIMITIVE(vm->stringClass, "byteCount", string_byteCount);
  PRIMITIVE(vm->stringClass, "codePointAt(1)", string_codePointAt);
  PRIMITIVE(vm->stringClass, "contains(1)", string_contains);
  PRIMITIVE(vm->stringClass, "endsWith(1)", string_endsWith);
  PRIMITIVE(vm->stringClass, "indexOf(1)", string_indexOf1);
  PRIMITIVE(vm->stringClass, "indexOf(2)", string_indexOf2);
  PRIMITIVE(vm->stringClass, "iterate(1)", string_iterate);
  PRIMITIVE(vm->stringClass, "iterateByte(1)", string_iterateByte);
  PRIMITIVE(vm->stringClass, "iteratorValue(1)", string_iteratorValue);
  PRIMITIVE(vm->stringClass, "startsWith(1)", string_startsWith);
  PRIMITIVE(vm->stringClass, "toString", string_toString);

  GET_CORE_CLASS(vm->listClass, "List");
  PRIM_STATIC(vm->listClass, "filled(2)", list_filled);
  PRIM_STATIC(vm->listClass, "new()", list_new);
  PRIMITIVE(vm->listClass, "get(1)", list_subscript);
  PRIMITIVE(vm->listClass, "set(2)", list_subscriptSet);
  PRIMITIVE(vm->listClass, "add(1)", list_add);
  PRIMITIVE(vm->listClass, "addCore(1)", list_addCore);
  PRIMITIVE(vm->listClass, "clear()", list_clear);
  PRIMITIVE(vm->listClass, "size", list_size);
  PRIMITIVE(vm->listClass, "insert(2)", list_insert);
  PRIMITIVE(vm->listClass, "iterate(1)", list_iterate);
  PRIMITIVE(vm->listClass, "iteratorValue(1)", list_iteratorValue);
  PRIMITIVE(vm->listClass, "removeAt(1)", list_removeAt);
  PRIMITIVE(vm->listClass, "remove(1)", list_removeValue);
  PRIMITIVE(vm->listClass, "indexOf(1)", list_indexOf);
  PRIMITIVE(vm->listClass, "swap(2)", list_swap);

  GET_CORE_CLASS(vm->rangeClass, "Range");
  PRIMITIVE(vm->rangeClass, "from", range_from);
  PRIMITIVE(vm->rangeClass, "to", range_to);
  PRIMITIVE(vm->rangeClass, "min", range_min);
  PRIMITIVE(vm->rangeClass, "max", range_max);
  PRIMITIVE(vm->rangeClass, "isInclusive", range_isInclusive);
  PRIMITIVE(vm->rangeClass, "iterate(1)", range_iterate);
  PRIMITIVE(vm->rangeClass, "iteratorValue(1)", range_iteratorValue);
  PRIMITIVE(vm->rangeClass, "toString", range_toString);

  ObjClass* sysClass;
  GET_CORE_CLASS(sysClass, "Sys");
  PRIM_STATIC(sysClass, "clock", sys_clock);
  PRIM_STATIC(sysClass, "gc()", sys_gc);
  PRIM_STATIC(sysClass, "writeString(1)", sys_writeString);

  // Some string objects were created before stringClass even existed. Those
  // strings have a NULL classObj, so that needs to be fixed.
  for (Obj* obj = vm->objects; obj != NULL; obj = obj->next) {
    if (obj->type == OBJ_STRING) obj->cls = vm->stringClass;
  }
}
