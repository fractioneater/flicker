#ifndef flicker_common_h
#define flicker_common_h

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// FEATURE TOGGLES

// TODO: Explain
#ifndef NAN_TAGGING
  #define NAN_TAGGING 1
#endif

// CURRENT MAINTENANCE FLAGS
// These will be removed after I know the new change works.

// There aren't any at the moment.

// DEBUG FLAGS

// Don't do any compiling, just print the tokens.
#define DEBUG_PRINT_TOKENS 0

// Print the bytecode instructions immediately after compiling.
// 0 to disable, 1 to print only user code, 2 to print everything.
#define DEBUG_PRINT_CODE 0

// Print the bytecode instructions as they run.
// 0 to disable, 1 to print only user code, 2 to print everything.
#define DEBUG_TRACE_EXECUTION 0

// Always run GC whenever the vm or compiler messes with memory.
#define DEBUG_STRESS_GC 0

// Log memory allocation and garbage collector runs.
#define DEBUG_LOG_GC 0

// Prevents the VM from initializing the core library. (Why would you do this?)
#define DEBUG_REMOVE_CORE 0

// COMPILER AND VM VALUES

#define MAX_CONSTANTS 0x7fff

#define MAX_PARAMETERS 16

#define MAX_METHOD_NAME 64

// This value includes the longest possible method name,
// a maximum of a 2 digit parameter count, and a null terminator.
#define MAX_METHOD_SIGNATURE (MAX_METHOD_NAME + 5)

#define UINT8_COUNT (UINT8_MAX + 1)

#endif
