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

VM vm;

// static Value inputNative(int argCount, Value* args) {
//   if (argCount > 1) {
//     // Arity error
//   } else if (argCount == 1) {
//     printValue(args[0]);
//   }

//   char* buffer = NULL;
//   uint64_t length;
//   int read;
//   read = getline(&buffer, &length, stdin);
//   if (read == -1) {
//     return OBJ_VAL(copyStringLength("", 0));
//   }

//   buffer[strcspn(buffer, "\r\n")] = '\0';

//   return OBJ_VAL(takeString(buffer, length));
// }

// static Value readFileNative(int argCount, Value* args) {
//   if (argCount != 1) {
//     // Arity error
//   }

//   if (IS_STRING(args[0])) {
//     FILE* file = fopen(AS_CSTRING(args[0]), "rb");
//     if (file == NULL) {
//       // Couldn't open file
//     }

//     fseek(file, 0L, SEEK_END);
//     size_t fileSize = ftell(file);
//     rewind(file);

//     char* buffer = ALLOCATE(char, fileSize + 1);
//     if (buffer == NULL) {
//       // Not enough memory
//     }

//     size_t bytesRead = fread(buffer, sizeof(char), fileSize, file);
//     if (bytesRead < fileSize) {
//       // Couldn't read file
//     }

//     buffer[bytesRead] = '\0';
//     fclose(file);

//     return OBJ_VAL(takeString(buffer, fileSize + 1));
//   }

//   return NONE_VAL;
// }

// static Value gcNative(int argCount, Value* args) {
//   if (argCount != 0) {
//     // Arity error
//   }

//   collectGarbage();
//   return NONE_VAL;
// }

// static Value clockNative(int argCount, Value* args) {
//   if (argCount != 0) {
//     // Arity error
//   }

//   return NUMBER_VAL((double)clock() / CLOCKS_PER_SEC);
// }

// static Value errorNative(int argCount, Value* args) {
//   if (argCount > 1) {
//     // Arity error
//   }

//   if (IS_STRING(args[0])) {
//     fprintf(stderr, "%s\n", AS_CSTRING(args[0]));
//   }

//   return NONE_VAL;
// }

// static Value listAddNative(int argCount, Value* args) {
//   if (argCount != 2) {
//     // Arity error
//   }

//   if (IS_LIST(args[0])) {
//     ObjList* list = AS_LIST(args[0]);
//     Value item = args[1];
//     listAppend(list, item);
//     return item;
//   }

//   return NONE_VAL;
// }

// static Value listDeleteNative(int argCount, Value* args) {
//   if (argCount != 2) {
//     // Arity error
//   }

//   if (IS_LIST(args[0]) && IS_NUMBER(args[1])) {
//     ObjList* list = AS_LIST(args[0]);
//     int index = AS_NUMBER(args[1]);

//     if (!isValidListIndex(list, index)) {
//       // Out of bounds error
//     }

//     return listDeleteAt(list, index);
//   }

//   return NONE_VAL;
// }

static void resetStack() {
  vm.stackTop = vm.stack;
  vm.frameCount = 0;
  vm.openUpvalues = NULL;
}

void runtimeError(const char* format, ...) {
  va_list args;
  va_start(args, format);
  vfprintf(stderr, format, args);
  va_end(args);
  fputs("\n", stderr);

  for (int i = vm.frameCount - 1; i >= 0; i--) {
    CallFrame* frame = &vm.frames[i];
    ObjFunction* function = frame->closure->function;
    size_t instruction = frame->ip - function->chunk.code - 1;
    fprintf(stderr, "[line %d] in ", function->chunk.lines[instruction]);
    if (function->name == NULL) {
      fprintf(stderr, "script\n");
    } else {
      fprintf(stderr, "%s()\n", function->name->chars);
    }
  }

  resetStack();
}

// static void defineNativeFunction(const char* name, NativeFn function) {
//   ObjString* fnName = copyString(name);
//   pushRoot((Obj*)fnName);
//   ObjNative* native = newNative(function);
//   pushRoot((Obj*)native);
//   tableSet(&vm.globals, fnName, OBJ_VAL(native));
//   popRoot();
//   popRoot();
// }

// static ObjClass* defineNativeClass(const char* name) {
//   // Store the class name and create the class.
//   ObjString* className = copyString(name);
//   ObjClass* cls = newSingleClass(className);
//   // Define the class as a global.
//   tableSet(&vm.globals, className, OBJ_VAL(cls));

//   return cls;
// }

// defineNativeMethod is gone.

void initVM() {
  resetStack();
  vm.objects = NULL;
  vm.bytesAllocated = 0;
  vm.nextGC = 1024 * 1024;

  vm.grayCount = 0;
  vm.grayCapacity = 0;
  vm.grayStack = NULL;

  initTable(&vm.globals);
  initTable(&vm.strings);

  vm.initString = NULL;
  vm.initString = copyStringLength("init", 4);

  // defineNativeFunction("input", inputNative);
  // defineNativeFunction("readFile", readFileNative);
  // defineNativeFunction("add", listAddNative);
  // defineNativeFunction("delete", listDeleteNative);

  // ObjClass* sysClass = defineNativeClass("Sys");

  // defineNativeMethod(sysClass, "gc", gcNative);
  // defineNativeMethod(sysClass, "clock", clockNative);
  // defineNativeMethod(sysClass, "error", errorNative);

#if !DEBUG_REMOVE_CORE
  initializeCore(&vm);
#endif
}

void freeVM() {
  freeTable(&vm.globals);
  freeTable(&vm.strings);
  vm.initString = NULL;
  freeObjects();
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

static Value peekInt(int distance) {
  return vm.stackTop[-1 - distance];
}

static bool isFalsy(Value value) {
  if (IS_NUMBER(value) && AS_NUMBER(value) == 0) return true;
  return IS_NONE(value) || (IS_BOOL(value) && !AS_BOOL(value));
}

static bool call(ObjClosure* closure, int argCount) {
  if (argCount != closure->function->arity) {
    runtimeError("Expected %d argument%s but got %d", closure->function->arity,
      closure->function->arity == 1 ? "" : "s", argCount);
    return false;
  }

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

static bool callValue(Value callee, int argCount) {
  if (IS_OBJ(callee)) {
    switch (OBJ_TYPE(callee)) {
      case OBJ_BOUND_METHOD: {
        ObjBoundMethod* bound = AS_BOUND_METHOD(callee);
        vm.stackTop[-argCount - 1] = bound->receiver;
        return call(bound->method, argCount);
      }
      case OBJ_CLASS: {
        ObjClass* cls = AS_CLASS(callee);
        vm.stackTop[-argCount - 1] = OBJ_VAL(newInstance(cls));
        Value initializer;
        if (tableGet(&cls->methods, vm.initString, &initializer)) {
          return call(AS_CLOSURE(initializer), argCount);
        } else if (argCount != 0) {
          runtimeError("Expected 0 arguments but got %d", argCount);
          return false;
        }
        return true;
      }
      case OBJ_CLOSURE:
        return call(AS_CLOSURE(callee), argCount);
      case OBJ_NATIVE: {
        ObjNative* nativeObj = AS_NATIVE(callee);
        if (!nativeObj->isPrimitive) {
          Value result = nativeObj->as.native(argCount, vm.stackTop - argCount);
          vm.stackTop -= argCount + 1;
          push(result);
        } else {
          // TODO: Calling primitive functions
        }
        return true;
      }
      default:
        break; // Non-callable object type.
    }
  }
  runtimeError("Can only call functions and classes");
  return false;
}

static bool invoke(ObjString* name, int argCount) {
  Value receiver = peekInt(argCount);

  ObjClass* cls = getClass(receiver);

  if (IS_INSTANCE(receiver)) {
    ObjInstance* instance = AS_INSTANCE(receiver);

    Value field;
    if (tableGet(&instance->fields, name, &field)) {
      vm.stackTop[-argCount - 1] = field;
      return callValue(field, argCount);
    }
  }

  Value method;
  if (!tableGet(&cls->methods, name, &method)) {
    runtimeError("%s does not implement '%s'", cls->name->chars, name->chars);
    return false;
  }

  if (IS_NATIVE(method)) {
    ObjNative* nativeObj = AS_NATIVE(method);

    if (!nativeObj->isPrimitive) {
      Value result = nativeObj->as.native(argCount, vm.stackTop - argCount);
      vm.stackTop -= argCount + 1;
      push(result);
    } else {
      bool success = nativeObj->as.primitive(vm.stackTop - argCount - 1);
      vm.stackTop -= argCount;
      return success;
    }
    return true;
  }

  return call(AS_CLOSURE(method), argCount);
}

static bool bindMethod(ObjClass* cls, ObjString* name) {
  Value method;
  if (!tableGet(&cls->methods, name, &method)) {
    runtimeError("Undefined property '%s'", name->chars);
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

static void defineMethod(ObjString* name) {
  Value method = peek();
  ObjClass* cls = AS_CLASS(peek2());
  tableSet(&cls->methods, name, method);
  pop();
}

static void defineClassMethod(ObjString* name) {
  Value method = peek();
  ObjClass* cls = AS_CLASS(peek2());
  printf("<<<<<<<<%s metaclass: %s>>>>>>>>\n", cls->name->chars, cls->obj.cls == NULL ? "NULL" : "good");
  tableSet(&cls->obj.cls->methods, name, method);
  pop();
}

static InterpretResult run() {
  CallFrame* frame = &vm.frames[vm.frameCount - 1];
  register uint8_t* ip = frame->ip;

#define READ_BYTE() (*ip++)
#define READ_SHORT() (ip += 2, (uint16_t)((ip[-2] << 8) | ip[-1]))

#define READ_CONSTANT() (frame->closure->function->chunk.constants.values[READ_BYTE()])

#define READ_STRING() AS_STRING(READ_CONSTANT())
#define BINARY_OP(valueType, op)                          \
  do {                                                    \
    if (!IS_NUMBER(peek()) || !IS_NUMBER(peek2())) {      \
      frame->ip = ip;                                     \
      runtimeError("Operands must be numbers");           \
      return INTERPRET_RUNTIME_ERROR;                     \
    }                                                     \
    double b = AS_NUMBER(pop());                          \
    double a = AS_NUMBER(pop());                          \
    push(valueType(a op b));                              \
  } while (false)
#define BINARY_OP_INTS(valueType, op)                     \
  do {                                                    \
    if (!IS_NUMBER(peek() || !IS_NUMBER(peek2()))) {      \
      frame->ip = ip;                                     \
      runtimeError("Operands must be numbers");           \
      return INTERPRET_RUNTIME_ERROR;                     \
    }                                                     \
    double b = AS_NUMBER(pop());                          \
    double a = AS_NUMBER(pop());                          \
    if (!isInt(a) || !isInt(b)) {                         \
      frame->ip = ip;                                     \
      runtimeError("Operands must be integers");          \
      return INTERPRET_RUNTIME_ERROR;                     \
    }                                                     \
    push(valueType((int)trunc(a) op (int)trunc(b)));      \
  } while (false)

  for (;;) {
#if DEBUG_TRACE_EXECUTION
    printf("        ");
    for (Value* slot = vm.stack; slot < vm.stackTop; slot++) {
      printf("[ ");
      printValue(*slot);
      printf(" ]");
    }
    printf("\n");
    disassembleInstruction(&frame->closure->function->chunk, (int)(ip - frame->closure->function->chunk.code));
#endif
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
      case OP_DUP: push(peek()); break;
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
        if (!tableGet(&vm.globals, name, &value)) {
          frame->ip = ip;
          runtimeError("Undefined variable '%s'", name->chars);
          return INTERPRET_RUNTIME_ERROR;
        }
        push(value);
        break;
      }
      case OP_DEFINE_GLOBAL: {
        ObjString* name = READ_STRING();
        tableSet(&vm.globals, name, peek());
        pop();
        break;
      }
      case OP_SET_GLOBAL: {
        ObjString* name = READ_STRING();
        if (tableSet(&vm.globals, name, peek())) {
          tableDelete(&vm.globals, name);
          frame->ip = ip;
          runtimeError("Undefined variable '%s'", name->chars);
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
        ObjString* property = READ_STRING();

        ObjClass* cls = getClass(receiver);

        if (IS_INSTANCE(receiver)) {
          ObjInstance* instance = AS_INSTANCE(receiver);

          Value value;
          if (tableGet(&instance->fields, property, &value)) {
            pop(); // Instance
            push(value);
            break;
          }

          if (bindMethod(instance->obj.cls, property)) break;
        }

        Value method;
        if (!tableGet(&cls->methods, property, &method)) {
          frame->ip = ip;
          runtimeError("%s does not implement '%s'", cls->name->chars, property->chars);
          return INTERPRET_RUNTIME_ERROR;
        }

        if (IS_NATIVE(method)) {
          ObjNative* nativeObj = AS_NATIVE(method);
          if (!nativeObj->as.primitive(&receiver)) {
            return INTERPRET_RUNTIME_ERROR;
          }
          break;
        }

        if (!call(AS_CLOSURE(method), 0)) {
          return INTERPRET_RUNTIME_ERROR;
        }

        pop(); // The object (whatever it is)
        break;
      }
      case OP_SET_PROPERTY: {
        if (!IS_INSTANCE(peek2())) {
          frame->ip = ip;
          runtimeError("Only instances have fields");
          return INTERPRET_RUNTIME_ERROR;
        }

        ObjInstance* instance = AS_INSTANCE(peek2());
        tableSet(&instance->fields, READ_STRING(), peek());
        Value value = pop();
        pop();
        push(value);
        break;
      }
      case OP_GET_SUPER: {
        ObjString* name = READ_STRING();
        ObjClass* superclass = AS_CLASS(pop());

        if (!bindMethod(superclass, name)) {
          return INTERPRET_RUNTIME_ERROR;
        }
        break;
      }
#if !METHOD_CALL_OPERATORS
      case OP_EQUAL: {
        Value b = pop();
        Value a = pop();
        push(BOOL_VAL(valuesEqual(a, b)));
        break;
      }
      case OP_NOT_EQUAL: {
        Value b = pop();
        Value a = pop();
        push(BOOL_VAL(!valuesEqual(a, b)));
        break;
      }
      case OP_GREATER:       BINARY_OP(BOOL_VAL, >); break;
      case OP_GREATER_EQUAL: BINARY_OP(BOOL_VAL, >=); break;
      case OP_LESS:          BINARY_OP(BOOL_VAL, <); break;
      case OP_LESS_EQUAL:    BINARY_OP(BOOL_VAL, <=); break;
      case OP_BIT_OR:        BINARY_OP_INTS(NUMBER_VAL, |); break;
      case OP_BIT_XOR:       BINARY_OP_INTS(NUMBER_VAL, ^); break;
      case OP_BIT_AND:       BINARY_OP_INTS(NUMBER_VAL, &); break;
      case OP_SHL:           BINARY_OP_INTS(NUMBER_VAL, <<); break;
      case OP_SHR:           BINARY_OP_INTS(NUMBER_VAL, >>); break;
      case OP_RANGE_EXCL: {
        if (!IS_NUMBER(peek()) || !IS_NUMBER(peek2())) {
          runtimeError("Operands must be numbers");
          return INTERPRET_RUNTIME_ERROR;
        }
        double to = AS_NUMBER(pop());
        double from = AS_NUMBER(pop());
        if (!isInt(to) || !isInt(from)) {
          runtimeError("Operands must be integers");
          return INTERPRET_RUNTIME_ERROR;
        }
        push(OBJ_VAL(newRange(from, to, false)));
        break;
      }
      case OP_RANGE_INCL: {
        if (!IS_NUMBER(peek()) || !IS_NUMBER(peek2())) {
          runtimeError("Operands must be numbers");
          return INTERPRET_RUNTIME_ERROR;
        }
        double to = AS_NUMBER(pop());
        double from = AS_NUMBER(pop());
        if (!isInt(to) || !isInt(from)) {
          runtimeError("Operands must be integers");
          return INTERPRET_RUNTIME_ERROR;
        }
        push(OBJ_VAL(newRange(from, to, true)));
        break;
      }
      case OP_ADD: {
        if (IS_STRING(peek()) && IS_STRING(peek2())) {
          concatenate();
        } else if (IS_NUMBER(peek()) && IS_NUMBER(peek2())) {
          double b = AS_NUMBER(pop());
          double a = AS_NUMBER(pop());
          push(NUMBER_VAL(a + b));
        } else {
          frame->ip = ip;
          runtimeError("Operands must be two numbers or two strings");
          return INTERPRET_RUNTIME_ERROR;
        }
        break;
      }
      case OP_SUBTRACT: BINARY_OP(NUMBER_VAL, -); break;
      case OP_MULTIPLY: BINARY_OP(NUMBER_VAL, *); break;
      case OP_DIVIDE:   BINARY_OP(NUMBER_VAL, /); break;
      case OP_MODULO: {
        if (!IS_NUMBER(peek()) || !IS_NUMBER(peek2())) {
          frame->ip = ip;
          runtimeError("Operands must be numbers");
          return INTERPRET_RUNTIME_ERROR;
        }
        double b = AS_NUMBER(pop());
        double a = AS_NUMBER(pop());
        double c = fmod(a, b);
        if ((c < 0 && b >= 0) || (b < 0 && a >= 0)) c += b;
        push(NUMBER_VAL(c));
        break;
      }
      case OP_EXPONENT: {
        if (!IS_NUMBER(peek()) || !IS_NUMBER(peek2())) {
          frame->ip = ip;
          runtimeError("Operands must be numbers");
          return INTERPRET_RUNTIME_ERROR;
        }
        double b = AS_NUMBER(pop());
        double a = AS_NUMBER(pop());
        push(NUMBER_VAL(pow(a, b)));
        break;
      }
      case OP_NOT:
        push(BOOL_VAL(isFalsy(pop())));
        break;
      case OP_NEGATE:
        if (!IS_NUMBER(peek())) {
          frame->ip = ip;
          runtimeError("Operand must be a number");
          return INTERPRET_RUNTIME_ERROR;
        }
        push(NUMBER_VAL(-AS_NUMBER(pop())));
        break;
#endif
      case OP_PRINT: {
        printValue(pop());
        printf("\n");
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
      case OP_LOOP: {
        uint16_t offset = READ_SHORT();
        ip -= offset;
        break;
      }
      case OP_CALL: {
        int argCount = READ_BYTE();
        frame->ip = ip;
        if (!callValue(peekInt(argCount), argCount)) {
          return INTERPRET_RUNTIME_ERROR;
        }
        frame = &vm.frames[vm.frameCount - 1];
        ip = frame->ip;
        break;
      }
      case OP_INVOKE: {
        ObjString* method = READ_STRING();
        int argCount = READ_BYTE();
        frame->ip = ip;
        if (!invoke(method, argCount)) {
          return INTERPRET_RUNTIME_ERROR;
        }
        frame = &vm.frames[vm.frameCount - 1];
        ip = frame->ip;
        break;
      }
      case OP_SUPER_INVOKE: {
        ObjString* name = READ_STRING();
        int argCount = READ_BYTE();
        ObjClass* superclass = AS_CLASS(pop());
        Value method;
        if (!tableGet(&superclass->methods, name, &method)) {
          runtimeError("Undefined method '%s'", name->chars);
          return INTERPRET_RUNTIME_ERROR;
        }

        if (!call(AS_CLOSURE(method), argCount)) {
          return INTERPRET_RUNTIME_ERROR;
        }
        frame = &vm.frames[vm.frameCount - 1];
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
          if (result != NONE_VAL) {
            printf("= > ");
            printValue(result);
            printf("\n");
          }
          pop();
          return INTERPRET_OK;
        }

        vm.stackTop = frame->slots;
        push(result);
        frame = &vm.frames[vm.frameCount - 1];
        ip = frame->ip;
        break;
      }
      case OP_CLASS: {
        Value superclass = peek();
        if (!IS_CLASS(superclass)) {
          frame->ip = ip;
          runtimeError("Superclass must be a class");
          return INTERPRET_RUNTIME_ERROR;
        }

        ObjClass* new = newClass(READ_STRING());
        bindSuperclass(new, AS_CLASS(superclass));

        pop(); // Superclass
        push(OBJ_VAL(new));
        break;
      }
      case OP_METHOD_INSTANCE:
        defineMethod(READ_STRING());
        break;
      case OP_METHOD_STATIC:
        defineClassMethod(READ_STRING());
        break;
    }
  }

#undef READ_BYTE
#undef READ_CONSTANT
#undef READ_SHORT
#undef READ_STRING
#undef BINARY_OP
#undef BINARY_OP_INTS
}

InterpretResult interpret(const char* source, const char* module) {
  ObjFunction* function = compile(source, module);
  if (function == NULL) return INTERPRET_COMPILE_ERROR;

#if DEBUG_PRINT_CODE
  printf("\n");
#endif

  pushRoot((Obj*)function);
  ObjClosure* closure = newClosure(function);
  popRoot();
  push(OBJ_VAL(closure));
  call(closure, 0);

  return run();
}
