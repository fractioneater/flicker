#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "chunk.h"
#include "common.h"
#include "debug.h"
#include "memory.h"
#include "vm.h"

static inline bool isSeparator(char c) {
  if (c == '/') return true;

#ifdef _WIN32
  if (c == '\\') return true;
#endif

  return false;
}

static char* removeExtension(const char* path) {
  size_t length = strlen(path);
  for (size_t i = length - 1; i >= 0; i--) {
    if (isSeparator(path[i])) {
      char* new = ALLOCATE(char, length + 1);
      return strncpy(new, path, length);
    }

    if (path[i] == '.') {
      char* new = ALLOCATE(char, i + 1);
      return strncpy(new, path, i);
    }
  }
  char* new = ALLOCATE(char, length + 1);
  return strncpy(new, path, length);
}

static void repl() {
  char line[1024];
  for (;;) {
    printf("~ > ");

    if (!fgets(line, sizeof(line), stdin)) {
      printf("\b\b\b\b");
      break;
    }

    interpret(line, "input", true);
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
  char* newPath = removeExtension(path);

  InterpretResult result = interpret(source, newPath, false);

  free(newPath);
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
