#ifndef flicker_utils_h
#define flicker_utils_h

#include "object.h"
#include "value.h"
#include "vm.h"

#define DECLARE_ARRAY(name, lower, type)                           \
  typedef struct {                                                 \
    type* data;                                                    \
    int count;                                                     \
    int capacity;                                                  \
  } name##Array;                                                   \
  void lower##ArrayInit(name##Array* array);                       \
  void lower##ArrayClear(name##Array* array);                      \
  void lower##ArrayFill(name##Array* array, type data, int count); \
  void lower##ArrayWrite(name##Array* array, type data);           \
  void lower##ArrayFree(name##Array* array)

#define DEFINE_ARRAY(name, lower, type)                                \
  void lower##ArrayInit(name##Array* array) {                          \
    array->data = NULL;                                                \
    array->capacity = 0;                                               \
    array->count = 0;                                                  \
  }                                                                    \
  void lower##ArrayClear(name##Array* array) {                         \
    reallocate(array->data, 0, 0);                                     \
    lower##ArrayInit(array);                                           \
  }                                                                    \
  void lower##ArrayFill(name##Array* array, type data, int count) {    \
    if (array->capacity < array->count + count) {                      \
      int oldCapacity = array->capacity;                               \
      array->capacity = GROW_CAPACITY(oldCapacity);                    \
      array->data =                                                    \
          GROW_ARRAY(type, array->data, oldCapacity, array->capacity); \
    }                                                                  \
    for (int i = 0; i < count; i++) {                                  \
      array->data[array->count++] = data;                              \
    }                                                                  \
  }                                                                    \
  void lower##ArrayWrite(name##Array* array, type data) {              \
    lower##ArrayFill(array, data, 1);                                  \
  }                                                                    \
  void lower##ArrayFree(name##Array* array) {                          \
    FREE_ARRAY(type, array->data, array->capacity);                    \
    lower##ArrayInit(array);                                           \
  }

DECLARE_ARRAY(Byte, byte, uint8_t);
DECLARE_ARRAY(Int, int, int);

char* simplifyPath(const char* path);

int utf8EncodeNumBytes(int value);
int utf8Encode(int value, uint8_t* bytes);
int utf8DecodeNumBytes(uint8_t byte);
int utf8Decode(const uint8_t* bytes, uint32_t length);

#endif
