#ifndef flicker_common_h
#define flicker_common_h

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// FEATURE TOGGLES

// Explanation of NaN Tagging
//
// Double precision floating point numbers are stored with 1 sign bit,
// 11 exponent bits, and 52 fraction bits. They are meant to store numbers,
// but they can also have a few other values like NaN ("not a number"),
// and negative and positive Infinity. To signify NaN, all exponent bits are
// set, like this:
//
// -11111111111----------------------------------------------------
//
// If NaN values only use those bits marked as 1, and all of the others don't
// matter, wouldn't there be a lot of possible values that are counted as NaN?
// Yes, there are. Flicker uses NaN tagging to take advantage of those
// possible values to represent things like True, False, None, and objects.
//
// There's one other thing, though. There are two types of NaN values, "quiet"
// and "signalling". Signalling NaNs are supposed to cause an error or stop
// execution, while quiet NaN values mostly don't interfere. We want to use
// the quiet version, because we don't want to mess up anything. To indicate
// a quiet NaN, the highest fraction bit it set.
//
// -[NaN      ]1---------------------------------------------------
//             ^ Quiet NaN bit
//
// So if all of those NaN bits are set, it's not a number, and we can use
// all of those other bits for a few things. We'll store special singleton values
// like "True", "False", and "None", as well as pointers to object on the heap.
// Flicker uses the sign bit to distinguish singleton values from pointers. If
// the sign bit it set, it's a pointer.
//
// S[NaN      ]1---------------------------------------------------
// ^ Singleton or pointer?
//
// There are only a few singleton values, so we'll just use the lowest 3 fraction
// bits to enumerate the possible values.
//
// 0[NaN      ]1------------------------------------------------[T]
//                                                  3 Type bits ^
//
// The last thing to include is pointers. We have 51 bits to use (remember, the
// lowest 3 bits don't matter unless the sign bit is 0), which is more than enough
// for a 32-bit address. It's also more than enough for 64-bit machines, because
// they only actually use 48 bits for addresses. To store them, we just put the
// pointer directly into the fraction bits.
//
// NaN tagging seems interesting, but it's more than just that. We have numbers (of
// course), singleton values, pointers to objects stored in one 64-bit sequence, and
// we don't even have to do any work to get numbers from these values, they're not
// masked or modified in any way.
#ifndef NAN_TAGGING
#  define NAN_TAGGING 1
#endif

// CURRENT MAINTENANCE FLAGS
// These will be removed after I know the new change works.

// There aren't any at the moment.

// DEBUG FLAGS

// Don't do any compiling, just print the tokens.
// 0 to disable, 1 to print only user code, 2 to print everything.
#define DEBUG_PRINT_TOKENS 0

// Print the bytecode instructions immediately after compiling.
// 0 to disable, 1 to print only user code, 2 to print everything.
#define DEBUG_PRINT_CODE 0

// Print the bytecode instructions as they run.
// 0 to disable, 1 to print only user code, 2 to print everything.
#define DEBUG_TRACE_EXECUTION 1

// Assertions are conditionals that should always return true (unless
// something is broken). Enabling slows down code, but will run those checks.
#define DEBUG_ENABLE_ASSERTIONS 1

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

#if DEBUG_ENABLE_ASSERTIONS

#  define ASSERT(condition, message)                                       \
    do {                                                                   \
      if (!(condition)) {                                                  \
        fprintf(stderr, "\033[1m%s:%d\033[0m assert failed in %s(): %s\n", \
                __FILE__, __LINE__, __func__, message);                    \
        abort();                                                           \
      }                                                                    \
    } while (false)

#else

#  define ASSERT(condition, message) do {} while (false)

#endif // DEBUG_ENABLE_ASSERTIONS
#endif // flicker_common_h
