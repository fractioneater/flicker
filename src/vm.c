#include "vm.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "common.h"
#include "compiler.h"
#include "core.h"
#include "debug.h"
#include "memory.h"
#include "object.h"
#include "utils.h"

VM vm;

// Forward declaration
static void resetStack();

void initVM() {
  resetStack();
  vm.objects = NULL;
  vm.bytesAllocated = 0;
  vm.nextGC = 1024 * 1024;

  vm.grayCount = 0;
  vm.grayCapacity = 0;
  vm.grayStack = NULL;

  initTable(&vm.strings);

  vm.initString = NULL;
  vm.initString = copyStringLength("init", 4);
  vm.coreString = NULL;
  vm.coreString = copyStringLength("core", 4);

# if DEBUG_REMOVE_CORE
  vm.coreInitialized = true;
# else
  vm.coreInitialized = false;
  initializeCore(&vm);
  vm.coreInitialized = true;
# endif
}

void freeVM() {
  freeTable(&vm.strings);

  for (int i = 0; i < vm.modules.capacity; i++) {
    if (vm.modules.entries[i].key != NULL) {
      freeTable(&AS_MODULE(vm.modules.entries[i].value)->variables);
    }
  }
  freeTable(&vm.modules);
  
  vm.initString = NULL;
  vm.coreString = NULL;
  freeObjects();
}

static void resetStack() {
  vm.stackTop = vm.stack;
  vm.frameCount = 0;
  vm.openUpvalues = NULL;
}

static inline void printTrace(ObjFunction* function, int line) {
  fprintf(stderr, "  line %d in \033[1m", line);
  if (function->name == NULL) {
    fprintf(stderr, "main\033[0m\n");
  } else if (function->name->length == 1 && function->name->chars[0] == '\b') {
    fprintf(stderr, "lambda { }\033[0m\n");
  } else {
    fprintf(stderr, "%s()\033[0m\n", function->name->chars);
  }
}

void runtimeError(const char* format, ...) {
  fprintf(stderr, "Traceback (most recent call last):\n");
  int repetitions = 0;
  int prevLine = 0;
  ObjFunction* prevFunction = NULL;
  for (int i = 0; i < vm.frameCount; i++) {
    CallFrame* frame = &vm.frames[i];
    ObjFunction* function = frame->closure->function;
    size_t instruction = frame->ip - function->chunk.code - 1;
    if (i != vm.frameCount - 1 && (function == prevFunction && function->chunk.lines[instruction] == prevLine)) {
      repetitions++;
    } else {
      // If calls are repeated, they can have up to 3 lines.
      if (repetitions > 2) {
        // No need to print it twice here, it's printed once before it gets
        // recognized as a repeat.
        printTrace(prevFunction, prevLine);
        fprintf(stderr, "  ... call repeated %d more times\n", repetitions - 1);
      } else {
        for (int j = 0; j < repetitions; j++) {
          printTrace(prevFunction, prevLine);
        }
      }

      printTrace(function, function->chunk.lines[instruction]);
      repetitions = 0;
      prevFunction = function;
      prevLine = function->chunk.lines[instruction];
    }
  }

  fprintf(stderr, "Error: ");
  va_list args;
  va_start(args, format);
  vfprintf(stderr, format, args);
  va_end(args);
  fputs("\n", stderr);

  resetStack();
}

static ObjModule* getModule(ObjString* name) {
  Value module;
  if (tableGet(&vm.modules, name, &module)) {
    return AS_MODULE(module);
  }
  return NULL;
}

static ObjClosure* compileInModule(const char* source, ObjString* name, bool printResult) {
  ASSERT(name != NULL, "Module name cannot be NULL");
  // Make sure it hasn't already been loaded.
  ObjModule* module = getModule(name);
  if (module == NULL) {
    module = newModule(name, false);
    pushRoot((Obj*)module);

    tableSet(&vm.modules, name, OBJ_VAL(module), true);
    popRoot();

    ObjModule* coreModule = getModule(vm.coreString);
    tableAddAll(&coreModule->variables, &module->variables, false);
  }

  ObjFunction* function = compile(source, module, printResult);
  if (function == NULL) {
    return NULL;
  }

  pushRoot((Obj*)function);
  ObjClosure* closure = newClosure(function);
  popRoot();

  return closure;
}

static Value importModule(ObjString* name) {
  Value existing;
  if (tableGet(&vm.modules, name, &existing)) return existing;

  pushRoot((Obj*)name);

  FILE* file = fopen(name->chars, "rb");
  if (file == NULL) {
    runtimeError("File '%s' does not exist", name->chars);
    popRoot();
    return NONE_VAL;
  }

  fseek(file, 0L, SEEK_END);
  size_t fileSize = ftell(file);
  rewind(file);

  char* buffer = (char*)malloc(fileSize + 1);
  if (buffer == NULL) {
    runtimeError("Not enough memory to import '%s'", name->chars);
    popRoot();
    return NONE_VAL;
  }

  size_t bytesRead = fread(buffer, sizeof(char), fileSize, file);
  if (bytesRead < fileSize) {
    runtimeError("Could not read file '%s'", name->chars);
    free(buffer);
    popRoot();
    return NONE_VAL;
  }

  buffer[bytesRead] = '\0';
  fclose(file);

  char* moduleChars = simplifyPath(name->chars);
  ObjString* moduleName = takeString(moduleChars, (int)strlen(moduleChars));
  pushRoot((Obj*)moduleName);

  ObjClosure* moduleClosure = compileInModule(buffer, moduleName, false);
  
  popRoot(); // moduleName
  free(buffer);
  popRoot(); // name

  if (moduleClosure == NULL) {
    runtimeError("Failed to compile module '%s'", name->chars);
    return NONE_VAL;
  }

  return OBJ_VAL(moduleClosure);
}

void push(Value value) {
  *vm.stackTop++ = value;
}

Value pop() {
  return *(--vm.stackTop);
}

static Value peek() {
  return vm.stackTop[-1];
}

static Value peek2() {
  return vm.stackTop[-2];
}

static Value peekN(int distance) {
  return vm.stackTop[-1 - distance];
}

static bool isFalsy(Value value) {
  return IS_NONE(value) || (IS_BOOL(value) && !AS_BOOL(value));
}

static bool call(ObjClosure* closure, int argCount) {
  if (vm.frameCount == FRAMES_MAX) {
    runtimeError("Stack overflow");
    return false;
  }

  CallFrame* frame = &vm.frames[vm.frameCount++];
  frame->closure = closure;
  frame->ip = closure->function->chunk.code;
  frame->slots = vm.stackTop - argCount - 1;
  return true;
}

static bool callArity(ObjClosure* closure, int argCount) {
  if (argCount != closure->function->arity) {
    runtimeError("Expected %d argument%s but got %d", closure->function->arity,
      closure->function->arity == 1 ? "" : "s", argCount);
    return false;
  }

  return call(closure, argCount);
}

static bool callNative(ObjNative* native, int argCount) {
  bool success = native->function(vm.stackTop - argCount - 1);
  if (success) vm.stackTop -= argCount;
  return success;
}

static bool callValue(Value callee, int argCount) {
  if (IS_OBJ(callee)) {
    switch (OBJ_TYPE(callee)) {
      case OBJ_BOUND_METHOD: {
        ObjBoundMethod* bound = AS_BOUND_METHOD(callee);
        vm.stackTop[-argCount - 1] = bound->receiver;
        return callArity(bound->method, argCount);
      }
      case OBJ_CLASS: {
        ObjClass* cls = AS_CLASS(callee);
        vm.stackTop[-argCount - 1] = OBJ_VAL(newInstance(cls));

        if (cls->initializer == UNDEFINED_VAL) {
          if (argCount != 0) {
            runtimeError("Expected 0 arguments but got %d", argCount);
            return false;
          }
          return true;
        }

        Value initializer = cls->initializer;
        if (IS_NATIVE(initializer)) {
          if (argCount != cls->arity) {
            runtimeError("Expected %d argument%s but got %d", cls->arity, cls->arity == 1 ? "" : "s", argCount);
            return false;
          }

          return callNative(AS_NATIVE(initializer), argCount);
        }

        ASSERT(IS_CLOSURE(initializer), "Initializer must be a native function or a closure");
        return callArity(AS_CLOSURE(initializer), argCount);
      }
      case OBJ_CLOSURE:
        return callArity(AS_CLOSURE(callee), argCount);
      case OBJ_NATIVE:
        return callNative(AS_NATIVE(callee), argCount);
      default:
        break; // Non-callable object type.
    }
  }
  runtimeError("Can only call functions and classes");
  return false;
}

static bool invokeFromClass(ObjClass* cls, ObjString* name, int argCount) {
  Value method;
  if (!tableGet(&cls->methods, name, &method)) {
    runtimeError("%s does not implement '%s'", cls->name->chars, name->chars);
    return false;
  }

  if (IS_NATIVE(method)) {
    return callNative(AS_NATIVE(method), argCount);
  }

  return call(AS_CLOSURE(method), argCount);
}

static bool invoke(ObjString* name, int argCount) {
  Value receiver = peekN(argCount);

  ObjClass* cls = getClass(receiver);
  ASSERT(cls != NULL, "Class cannot be NULL");

  // First check if the method is a field.
  if (IS_INSTANCE(receiver)) {
    ObjInstance* instance = AS_INSTANCE(receiver);
    ObjString* fieldName = copyStringLength(name->chars, name->length - (ceil(log10(argCount + 1)) + 2));

    Value field;
    if (tableGet(&instance->fields, fieldName, &field)) {
      vm.stackTop[-argCount - 1] = field;
      return callValue(field, argCount);
    }
  }

  // If it's not a field, try to invoke it as a method.
  return invokeFromClass(cls, name, argCount);
}

static bool bindMethod(ObjClass* cls, ObjString* name) {
  Value method;
  if (!tableGet(&cls->methods, name, &method)) {
    runtimeError("Undefined method '%s'", name->chars);
    return false;
  }

  if (IS_NATIVE(method)) {
    runtimeError("Cannot bind native method '%s'", name->chars);
    return false;
  }

  ObjBoundMethod* bound = newBoundMethod(peek(), AS_CLOSURE(method));
  pop();
  push(OBJ_VAL(bound));
  return true;
}

static ObjUpvalue* captureUpvalue(Value* local) {
  ObjUpvalue* prevUpvalue = NULL;
  ObjUpvalue* upvalue = vm.openUpvalues;
  while (upvalue != NULL && upvalue->location > local) {
    prevUpvalue = upvalue;
    upvalue = upvalue->next;
  }

  if (upvalue != NULL && upvalue->location == local) {
    return upvalue;
  }

  ObjUpvalue* createdUpvalue = newUpvalue(local);
  createdUpvalue->next = upvalue;

  if (prevUpvalue == NULL) {
    vm.openUpvalues = createdUpvalue;
  } else {
    prevUpvalue->next = createdUpvalue;
  }

  return createdUpvalue;
}

static void closeUpvalues(Value* last) {
  while (vm.openUpvalues != NULL && vm.openUpvalues->location >= last) {
    ObjUpvalue* upvalue = vm.openUpvalues;
    upvalue->closed = *upvalue->location;
    upvalue->location = &upvalue->closed;
    vm.openUpvalues = upvalue->next;
  }
}

static void defineMethod(ObjClass* cls, ObjString* name) {
  Value method = peek();
  tableSet(&cls->methods, name, method, true);
  pop();
}

#if DEBUG_TRACE_EXECUTION
static void printStack() {
  for (Value* slot = vm.stack; slot < vm.stackTop; slot++) {
    printf("[ ");
    printValue(*slot);
    printf(" ]");
  }
  printf("\n");
}
#endif

static InterpretResult run() {
  CallFrame* frame = &vm.frames[vm.frameCount - 1];
  register uint8_t* ip = frame->ip;

# define READ_BYTE() (*ip++)
# define READ_SHORT() (ip += 2, (uint16_t)((ip[-2] << 8) | ip[-1]))

# define READ_CONSTANT()                                               \
  (frame->closure->function->chunk.constants                           \
       .values[*ip++ >= 0x80 ? (ip++, ((ip[-2] & 0x7f) << 8) | ip[-1]) \
                             : ip[-1]])

# define READ_STRING() AS_STRING(READ_CONSTANT())

  for (;;) {
#   if DEBUG_TRACE_EXECUTION == 2
    printf("        ");
    printStack();
    disassembleInstruction(&frame->closure->function->chunk, (int)(ip - frame->closure->function->chunk.code));
#   elif DEBUG_TRACE_EXECUTION == 1
    if (vm.coreInitialized) {
      printf("        ");
      printStack();
      disassembleInstruction(&frame->closure->function->chunk, (int)(ip - frame->closure->function->chunk.code));
    }
#   endif
    uint8_t instruction;
    switch (instruction = READ_BYTE()) {
      case OP_CONSTANT: {
        Value constant = READ_CONSTANT();
        push(constant);
        break;
      }
      case OP_NONE: push(NONE_VAL); break;
      case OP_TRUE: push(BOOL_VAL(true)); break;
      case OP_FALSE: push(BOOL_VAL(false)); break;
      case OP_POP: pop(); break;
      case OP_DUP: push(peekN(READ_BYTE())); break;
      case OP_GET_LOCAL: {
        uint8_t slot = READ_BYTE();
        push(frame->slots[slot]);
        break;
      }
      case OP_SET_LOCAL: {
        uint8_t slot = READ_BYTE();
        frame->slots[slot] = peek();
        break;
      }
      case OP_GET_GLOBAL: {
        ObjString* name = READ_STRING();
        Value value;
        ObjModule* module = frame->closure->function->module;
        if (!tableGet(&module->variables, name, &value)) {
          frame->ip = ip;
          runtimeError("Undefined variable '%s'", name->chars);
          return INTERPRET_RUNTIME_ERROR;
        }
        push(value);
        break;
      }
      case OP_DEFINE_GLOBAL: {
        ObjString* name = READ_STRING();
        ObjModule* module = frame->closure->function->module;
        if (!tableSetMutable(&module->variables, name, pop(), true)) {
          frame->ip = ip;
          runtimeError("Conflicting declarations of value '%s'", name->chars);
          return INTERPRET_RUNTIME_ERROR;
        }
        break;
      }
      case OP_DEFINE_IMMUTABLE_GLOBAL: {
        ObjString* name = READ_STRING();
        ObjModule* module = frame->closure->function->module;
        if (!tableSetMutable(&module->variables, name, pop(), false)) {
          frame->ip = ip;
          runtimeError("Conflicting declarations of value '%s'", name->chars);
          return INTERPRET_RUNTIME_ERROR;
        }
        break;
      }
      case OP_SET_GLOBAL: {
        ObjString* name = READ_STRING();
        ObjModule* module = frame->closure->function->module;
        if (!tableContains(&module->variables, name)) {
          frame->ip = ip;
          runtimeError("Undefined variable '%s'", name->chars);
          return INTERPRET_RUNTIME_ERROR;
        }

        if (!tableSetMutable(&module->variables, name, peek(), true)) {
          frame->ip = ip;
          runtimeError("Value '%s' cannot be reassigned", name->chars);
          return INTERPRET_RUNTIME_ERROR;
        }
        break;
      }
      case OP_GET_UPVALUE: {
        uint8_t slot = READ_BYTE();
        push(*frame->closure->upvalues[slot]->location);
        break;
      }
      case OP_SET_UPVALUE: {
        uint8_t slot = READ_BYTE();
        *frame->closure->upvalues[slot]->location = peek();
        break;
      }
      case OP_GET_PROPERTY: {
        Value receiver = peek();
        ObjClass* cls = getClass(receiver);
        ASSERT(cls != NULL, "Class cannot be NULL");

        ObjString* property = READ_STRING();

        if (IS_INSTANCE(receiver)) {
          ObjInstance* instance = AS_INSTANCE(receiver);

          Value value;
          if (tableGet(&instance->fields, property, &value)) {
            pop(); // Instance
            push(value);
            break;
          }
        }

        Value attribute;
        if (!tableGet(&cls->methods, property, &attribute)) {
          frame->ip = ip;
          runtimeError("Undefined property '%s'", property->chars);
          return INTERPRET_RUNTIME_ERROR;
        }

        if (IS_NATIVE(attribute)) {
          ObjNative* native = AS_NATIVE(attribute);
          // Replaces the instance with the result.
          if (!native->function(vm.stackTop - 1)) {
            return INTERPRET_RUNTIME_ERROR;
          }
        } else {
          frame->ip = ip;
          if (!call(AS_CLOSURE(attribute), 0)) {
            return INTERPRET_RUNTIME_ERROR;
          }
          frame = &vm.frames[vm.frameCount - 1];
          ip = frame->ip;
        }
        break;
      }
      case OP_SET_PROPERTY: {
        if (!IS_INSTANCE(peek2())) {
          frame->ip = ip;
          runtimeError("Only instances have fields");
          return INTERPRET_RUNTIME_ERROR;
        }

        ObjInstance* instance = AS_INSTANCE(peek2());
        tableSet(&instance->fields, READ_STRING(), peek(), true);
        Value value = pop();
        pop();
        push(value);
        break;
      }
      case OP_BIND_METHOD: {
        Value value = peek();
        ObjClass* cls = getClass(value);

        frame->ip = ip;
        if (cls == NULL) {
          runtimeError("Value does not belong to a class");
          return INTERPRET_RUNTIME_ERROR;
        }

        if (!bindMethod(cls, READ_STRING())) {
          return INTERPRET_RUNTIME_ERROR;
        }

        break;
      }
      case OP_BIND_SUPER: {
        ObjClass* superclass = getClass(peek());
        ASSERT(superclass != NULL, "Superclass cannot be NULL");

        ObjString* method = READ_STRING();

        frame->ip = ip;
        if (!bindMethod(superclass, method)) {
          return INTERPRET_RUNTIME_ERROR;
        }

        break;
      }
      case OP_PRINT: {
        Value output = peek();
        if (IS_STRING(output)) {
          printf("%s\n", AS_STRING(output)->chars);
        } else {
          printf("%s\n", "[invalid toString() method]");
        }
        pop(); // The string
        break;
      }
      case OP_ERROR: {
        Value output = peek();
        frame->ip = ip;
        if (IS_STRING(output)) {
          runtimeError(AS_CSTRING(output));
        } else {
          runtimeError("[invalid toString() method]");
        }
        return INTERPRET_RUNTIME_ERROR;
        break;
      }
      case OP_JUMP: {
        uint16_t offset = READ_SHORT();
        ip += offset;
        break;
      }
      case OP_JUMP_FALSY: {
        uint16_t offset = READ_SHORT();
        if (isFalsy(peek())) ip += offset;
        break;
      }
      case OP_JUMP_TRUTHY: {
        uint16_t offset = READ_SHORT();
        if (!isFalsy(peek())) ip += offset;
        break;
      }
      case OP_JUMP_TRUTHY_POP: {
        uint16_t offset = READ_SHORT();
        if (!isFalsy(peek())) ip += offset;
        pop();
        break;
      }
      case OP_LOOP: {
        uint16_t offset = READ_SHORT();
        ip -= offset;
        break;
      }
      case OP_CALL_0: case OP_CALL_1: case OP_CALL_2: case OP_CALL_3: case OP_CALL_4:
      case OP_CALL_5: case OP_CALL_6: case OP_CALL_7: case OP_CALL_8: case OP_CALL_9:
      case OP_CALL_10: case OP_CALL_11: case OP_CALL_12: case OP_CALL_13:
      case OP_CALL_14: case OP_CALL_15: case OP_CALL_16: {
        int argCount = instruction - OP_CALL_0;
        frame->ip = ip;
        if (!callValue(peekN(argCount), argCount)) {
          return INTERPRET_RUNTIME_ERROR;
        }
        frame = &vm.frames[vm.frameCount - 1];
        ip = frame->ip;
        break;
      }
      case OP_INVOKE_0: case OP_INVOKE_1: case OP_INVOKE_2: case OP_INVOKE_3: case OP_INVOKE_4:
      case OP_INVOKE_5: case OP_INVOKE_6: case OP_INVOKE_7: case OP_INVOKE_8: case OP_INVOKE_9:
      case OP_INVOKE_10: case OP_INVOKE_11: case OP_INVOKE_12: case OP_INVOKE_13:
      case OP_INVOKE_14: case OP_INVOKE_15: case OP_INVOKE_16: {
        int argCount = instruction - OP_INVOKE_0;
        ObjString* method = READ_STRING();
        frame->ip = ip;
        if (!invoke(method, argCount)) {
          return INTERPRET_RUNTIME_ERROR;
        }
        frame = &vm.frames[vm.frameCount - 1];
        ip = frame->ip;
        break;
      }
      case OP_SUPER_0: case OP_SUPER_1: case OP_SUPER_2: case OP_SUPER_3: case OP_SUPER_4:
      case OP_SUPER_5: case OP_SUPER_6: case OP_SUPER_7: case OP_SUPER_8: case OP_SUPER_9:
      case OP_SUPER_10: case OP_SUPER_11: case OP_SUPER_12: case OP_SUPER_13:
      case OP_SUPER_14: case OP_SUPER_15: case OP_SUPER_16: {
        int argCount = instruction - OP_SUPER_0;
        ObjString* name = READ_STRING();
        ObjClass* superclass = AS_CLASS(pop());
        frame->ip = ip;
        if (!invokeFromClass(superclass, name, argCount)) {
          return INTERPRET_RUNTIME_ERROR;
        }
        frame = &vm.frames[vm.frameCount - 1];
        ip = frame->ip;
        break;
      }
      case OP_IMPORT_MODULE: {
        frame->ip = ip;
        Value module = importModule(READ_STRING());
        if (IS_NONE(module)) return INTERPRET_RUNTIME_ERROR;
        push(module);

        if (IS_CLOSURE(module)) {
          frame->ip = ip;
          ObjClosure* closure = AS_CLOSURE(module);
          call(closure, 0);
          frame = &vm.frames[vm.frameCount - 1];
          ip = frame->ip;
        } else {
          vm.lastModule = AS_MODULE(module);
        }

        break;
      }
      case OP_IMPORT_VARIABLE: {
        ObjString* name = READ_STRING();
        ASSERT(vm.lastModule != NULL, "Module should be imported already");
        Value result;
        if (!tableGet(&vm.lastModule->variables, name, &result)) {
          frame->ip = ip;
          runtimeError("Could not find variable '%s' in module '%s'", name->chars, vm.lastModule->name->chars);
          return INTERPRET_RUNTIME_ERROR;
        }

        push(result);
        break;
      }
      case OP_END_MODULE: 
        vm.lastModule = frame->closure->function->module;
        break;
      case OP_TUPLE: {
        int length = READ_BYTE();
        ObjTuple* tuple = newTuple(length);

        for (int i = length - 1; i >= 0; i--) {
          tuple->items[i] = pop();
        }
        
        push(OBJ_VAL(tuple));
        break;
      }
      case OP_CLOSURE: {
        ObjFunction* function = AS_FUNCTION(READ_CONSTANT());
        ObjClosure* closure = newClosure(function);
        push(OBJ_VAL(closure));
        for (int i = 0; i < closure->upvalueCount; i++) {
          uint8_t isLocal = READ_BYTE();
          uint8_t index = READ_BYTE();
          if (isLocal) {
            closure->upvalues[i] = captureUpvalue(frame->slots + index);
          } else {
            closure->upvalues[i] = frame->closure->upvalues[index];
          }
        }
        break;
      }
      case OP_CLOSE_UPVALUE:
        closeUpvalues(vm.stackTop - 1);
        pop();
        break;
      case OP_RETURN: {
        Value result = pop();
        closeUpvalues(frame->slots);
        vm.frameCount--;
        if (vm.frameCount == 0) {
          pop(); // Main
          return INTERPRET_OK;
        }

        vm.stackTop = frame->slots;
        push(result);
        frame = &vm.frames[vm.frameCount - 1];
        ip = frame->ip;
        break;
      }
      case OP_RETURN_OUTPUT:
        printf("= > ");
        printValue(peek());
        printf("\n");
        break;
      case OP_CLASS: {
        Value superclass = peek();
        if (!IS_CLASS(superclass)) {
          frame->ip = ip;
          runtimeError("Superclass must be a class");
          return INTERPRET_RUNTIME_ERROR;
        }

        ObjClass* new = newClass(READ_STRING());
        bindSuperclass(new, AS_CLASS(superclass));

        push(OBJ_VAL(new));
        break;
      }
      case OP_INITIALIZER: {
        ObjClass* cls = AS_CLASS(peek2());
        cls->initializer = pop();
        break;
      }
      case OP_METHOD_INSTANCE:
        defineMethod(AS_CLASS(peek2()), READ_STRING());
        break;
      case OP_METHOD_STATIC:
        defineMethod(AS_CLASS(peek2())->obj.cls, READ_STRING());
        break;
    }
  }

# undef READ_BYTE
# undef READ_CONSTANT
# undef READ_SHORT
# undef READ_STRING
}

InterpretResult interpret(const char* source, const char* module, bool printResult) {
  ASSERT(module != NULL, "Module name should not be NULL");
  ObjString* moduleName;
  if (strlen(module) == 4 && memcmp(module, vm.coreString, 4) == 0) {
    moduleName = vm.coreString;
  } else moduleName = copyString(module);

  pushRoot((Obj*)moduleName);

  ObjClosure* closure = compileInModule(source, moduleName, printResult);

  popRoot(); // moduleName

  if (closure == NULL) return INTERPRET_COMPILE_ERROR;

  // Put a linebreak between the code and the execution just to be nice.
# if DEBUG_PRINT_CODE == 2 && DEBUG_TRACE_EXECUTION == 2
  printf("\n");
# endif

  push(OBJ_VAL(closure));
  // Initialize the main call frame
  call(closure, 0);

  return run();
}
