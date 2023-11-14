#include "utils.h"

#include <string.h>

#include "memory.h"

DEFINE_ARRAY(Byte, byte, uint8_t)
DEFINE_ARRAY(Int, int, int)

char* simplifyPath(const char* path) {
  // Remove extension
  char* withoutExtension = strdup(path);
  size_t length = strcspn(withoutExtension, ".");
  withoutExtension[length] = '\0';

  // Remove path
# ifdef _WIN32
  const char* forward = strrchr(withoutExtension, '/');
  const char* backward = strrchr(withoutExtension, '\\');

  char* value = strlen(forward) > strlen(backward) ? backward : forward ;
# else
  char* value = strrchr(withoutExtension, '/');
# endif
  if (value == NULL) return withoutExtension;

  // + 1 to leave out the slash character
  value = strdup(value + 1);
  free(withoutExtension);

  return value;
}

int utf8EncodeNumBytes(int value) {
  if (value <= 0x7f) return 1;
  if (value <= 0x7ff) return 2;
  if (value <= 0xffff) return 3;
  if (value <= 0x10ffff) return 4;
  return 0;
}

int utf8Encode(int value, uint8_t* bytes) {
  if (value <= 0x7f) {
    // One byte.
    *bytes = value & 0x7f;
    return 1;
  } else if (value <= 0x7ff) {
    // Two bytes: 110xxxxx 10xxxxxx
    *bytes = 0xc0 | ((value & 0x7c0) >> 6);
    bytes++;
    *bytes = 0x80 | (value & 0x3f);
    return 2;
  } else if (value <= 0xffff) {
    // Three bytes: 1110xxxx 10xxxxxx 10xxxxxx
    *bytes = 0xe0 | ((value & 0xf000) >> 12);
    bytes++;
    *bytes = 0x80 | ((value & 0xfc0) >> 6);
    bytes++;
    *bytes = 0x80 | (value & 0x3f);
    return 3;
  } else if (value <= 0x10ffff) {
    // Four bytes: 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
    *bytes = 0xf0 | ((value & 0x1c0000) >> 18);
    bytes++;
    *bytes = 0x80 | ((value & 0x3f000) >> 12);
    bytes++;
    *bytes = 0x80 | ((value & 0xfc0) >> 6);
    bytes++;
    *bytes = 0x80 | (value & 0x3f);
    return 4;
  }

  return 0;
}

int utf8DecodeNumBytes(uint8_t byte) {
  if ((byte & 0xc0) == 0x80) return 0;

  if ((byte & 0xf8) == 0xf0) return 4;
  if ((byte & 0xf0) == 0xe0) return 3;
  if ((byte & 0xe0) == 0xc0) return 2;
  return 1;
}

int utf8Decode(const uint8_t* bytes, uint32_t length) {
  // Single byte
  if (*bytes <= 0x7f) return *bytes;

  int value;
  uint8_t remaining;
  if ((*bytes & 0xe0) == 0xc0) {
    // Two bytes
    value = *bytes & 0x1f;
    remaining = 1;
  } else if ((*bytes & 0xf0) == 0xe0) {
    // Three bytes
    value = *bytes & 0x0f;
    remaining = 2;
  } else if ((*bytes & 0xf8) == 0xf0) {
    // Four bytes
    value = *bytes & 0x07;
    remaining = 3;
  } else {
    return -1;
  }

  if (remaining > length - 1) return -1;

  while (remaining > 0) {
    bytes++;
    remaining--;

    if ((*bytes & 0xc0) != 0x80) return -1;

    value = value << 6 | (*bytes & 0x3f);
  }

  return value;
}
