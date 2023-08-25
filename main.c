#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "chunk.h"
#include "common.h"
#include "debug.h"
#include "memory.h"
#include "vm.h"

typedef enum {
  // Examples: /home/username/directory/file.x OR C:\Directory\file.x
  PATH_TYPE_ABSOLUTE,

  // Examples: ./otherDirectory/file.x OR ../file.x
  PATH_TYPE_RELATIVE,

  // Examples: file.x OR directory/file.x
  PATH_TYPE_SIMPLE
} PathType;

typedef struct {
  // The array of chars for the path.
  char* chars;

  // The number of characters being used in chars (not including the null char at the end).
  size_t length;

  // The size of the allocated buffer for chars.
  size_t capacity;
} Path;

typedef struct {
  // I don't think I need to label these.
  const char* start;
  const char* end;
} Slice;

static void ensureCapacity(Path* path, size_t capacity) {
  // Account for the null byte at the end.
  capacity++;

  if (path->capacity >= capacity) return;

  size_t newCapacity = 16;
  while (newCapacity < capacity) newCapacity *= 2;

  path->chars = (char*)realloc(path->chars, newCapacity);
  path->capacity = newCapacity;
}

static void appendSlice(Path* path, Slice slice) {
  size_t length = slice.end - slice.start;
  ensureCapacity(path, path->length + length);
  memcpy(path->chars + path->length, slice.start, length);
  path->length += length;
  path->chars[path->length] = '\0';
}

static void pathAppendString(Path* path, const char* string) {
  Slice slice;
  slice.start = string;
  slice.end = string + strlen(string);
  appendSlice(path, slice);
}

static inline bool isSeparator(char c) {
  if (c == '/') return true;

#ifdef _WIN32
  if (c == '\\') return true;
#endif

  return false;
}

#ifdef _WIN32
static inline bool isDriveLetter(char c) {
  return ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z');
}
#endif

// static inline size_t absolutePrefixLength(const char* path) {
//   #ifdef _WIN32
//   if (isDriveLetter(path[0]) && path[1] == ':') {
//     if (isSeparator(path[2])) {
//       // Completely absolute.
//       return 3;
//     } else {
//       // Partially absolute path.
//       return 2;
//     }
//   }
//   #endif

//   // Non-Windows path or some Windows absolute paths.
//   if (isSeparator(path[0])) return 1;

//   // Not an absolute path.
//   return 0;
// }

// static PathType pathType(const char* path) {
//   if (absolutePrefixLength(path) > 0) return PATH_TYPE_ABSOLUTE;

//   // Check if it's relative.
//   if ((path[0] == '.' && isSeparator(path[1])) ||
//     (path[0] == '.' && path[1] == '.' && isSeparator(path[2]))
//   ) {
//     return PATH_TYPE_RELATIVE;
//   }

//   return PATH_TYPE_SIMPLE;
// }

static Path* pathNew(const char* string) {
  Path* path = (Path*)malloc(sizeof(Path));
  path->chars = (char*)malloc(1);
  path->chars[0] = '\0';
  path->length = 0;
  path->capacity = 0;

  pathAppendString(path, string);

  return path;
}

static void pathFree(Path* path) {
  if (path->chars) free(path->chars);
  free(path);
}

// static void pathRemoveExtension(Path* path) {
//   for (size_t i = path->length - 1; i < path->length; i--) {
//     if (isSeparator(path->chars[i])) return;

//     if (path->chars[i] == '.') {
//       path->length = i;
//       path->chars[path->length] = '\0';
//     }
//   }
// }

// static void pathAppendChar(Path* path, char c) {
//   ensureCapacity(path, path->length + 1);
//   path->chars[path->length++] = c;
//   path->chars[path->length] = '\0';
// }

// static void pathJoin(Path* path, const char* string) {
//   if (path->length > 0 && !isSeparator(path->chars[path->length - 1])) {
//     pathAppendChar(path, '/');
//   }

//   pathAppendString(path, string);
// }

static void repl() {
  char line[1024];
  for (;;) {
    printf("~ > ");

    if (!fgets(line, sizeof(line), stdin)) {
      printf("\n");
      break;
    }

    interpret(line, "<input>");
  }
}

static char* readFile(const char* path) {
  FILE* file = fopen(path, "rb");
  if (file == NULL) {
    fprintf(stderr, "Could not open file \"%s\".\n", path);
    exit(74);
  }

  fseek(file, 0L, SEEK_END);
  size_t fileSize = ftell(file);
  rewind(file);

  char* buffer = (char*)malloc(fileSize + 1);
  if (buffer == NULL) {
    fprintf(stderr, "Not enough memory to read \"%s\".\n", path);
    exit(74);
  }

  size_t bytesRead = fread(buffer, sizeof(char), fileSize, file);
  if (bytesRead < fileSize) {
    fprintf(stderr, "Could not read file \"%s\".\n", path);
    exit(74);
  }

  buffer[bytesRead] = '\0';

  fclose(file);
  return buffer;
}

static void runFile(const char* path) {
  char* source = readFile(path);

  Path* module = pathNew(path);

  // To make it a valid non-simple path:
  //
  // if (pathType(module->chars) == PATH_TYPE_SIMPLE) {
  //   Path* relative = pathNew(".");
  //   pathJoin(relative, path);

  //   pathFree(module);
  //   module = relative;
  // }

  // pathRemoveExtension(module);

  InterpretResult result = interpret(source, module->chars);

  pathFree(module);
  free(source);

  if (result == INTERPRET_COMPILE_ERROR) exit(65);
  if (result == INTERPRET_RUNTIME_ERROR) exit(70);
}

int main(int argc, const char* argv[]) {
  initVM();

  if (argc == 1) {
    repl();
  } else if (argc == 2) {
    runFile(argv[1]);
  } else {
    fprintf(stderr, "Usage: flicker [path]\n");
    exit(64);
  }

  freeVM();
  return 0;
}
