// This header file contains all of the utility functions and macros that are
// utilized thorughout the project

#pragma once
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include <pthread.h>

#define maybe_unused __attribute__((unused))

// Get some C RIAA

// This macro is used so that we can add a defer(func) attribute to a
// declaration and have it automatically call the function when the
// variable goes out of scope
#define defer(func) __attribute__((cleanup(func##_defer)))

// The __attribute__((cleanup)) requires a pointer to the variable
// which means a common cleanup function of jsut taking the variable
// directly won't work. So we have to create a wrapper function. This
// macro does that for us
#define declare_defer_func(func, type) \
  static maybe_unused void func##_defer(type* _defer) {    \
    func(*_defer);                     \
  }

// Another pitfall with defer is that sometimes you have to defer a
// cleanup with a guard. For example, maybe you only want to call the
// cleanup function if the variable is not NULL. This macro allows you to
// easily create a wrapper function for the defer
#define declare_defer_func_unless_null(func, type) \
  static maybe_unused void func##_defer(type* _defer) {                \
    if (*_defer) {                                 \
      func(*_defer);                               \
    }                                              \
  }

// Similar to the above except it checks if the value is non-positive
// instead of NULL. This is useful for things like file descriptors
#define declare_defer_func_unless_nonpositive(func, type) \
  static maybe_unused void func##_defer(type* _defer) {                       \
    if (*_defer > 0) {                                    \
      func(*_defer);                                      \
    }                                                     \
  }

declare_defer_func_unless_nonpositive(close, int);
declare_defer_func(free, void*);
declare_defer_func_unless_null(fclose, FILE*);

// These are nice init functions for things that we want to initialize that
// can't be done at compile time (yet, cmon constexpr function calls!)
#define constructor __attribute__((constructor))
#define destructor __attribute__((destructor))

// Test IPv6 and have -4 and -6 flags
// Add statistics collection and printing

#define LOG(fmt, ...) fprintf(stderr, fmt "\n", ##__VA_ARGS__)

#ifdef DEBUG_ENABLED
#define LOG_DEBUG(fmt, ...) fprintf(stderr, "%s(): %s:%d: " fmt "\n", __FUNCTION__, __FILE__, __LINE__, ##__VA_ARGS__)
#else
#define LOG_DEBUG(fmt, ...) \
  do {                      \
  } while (0)
#endif

#define MEM_DUMP(region_ptr, size) \
  do {                      \
    for (int i = 0; i < size; i++) { \
      printf("%02x ", ((unsigned char*)region_ptr)[i]); \
    }                      \
    printf("\n");          \
  } while (0)

// The following two macros are for facilitating the logging of error messages
// You wrap calls to expressions that return an int or a pointer which is
// supposed to indicate success or failure. For example, it's common for
// syscalls to return -1 on error and set errno. You can use
// PERROR_IF_NEG(syscall(...)) to log the error message if the syscall fails.
#define PERROR_IF_NEG(expr)                                    \
  ({                                                           \
    int __err = (expr);                                        \
    if (__err < 0) {                                           \
      LOG("%s: %s: %s", __FUNCTION__, #expr, strerror(errno)); \
    }                                                          \
    __err;                                                     \
  })

// Same as above but for pointer return values, if a call returns null to
// indicate an errno, you can use this macro to log the error message
#define PERROR_IF_NULL(expr)                                   \
  ({                                                           \
    void* __err = (expr);                                      \
    if (__err == NULL) {                                       \
      LOG("%s: %s: %s", __FUNCTION__, #expr, strerror(errno)); \
    }                                                          \
    __err;                                                     \
  })

// Simple helper macro to check the return value of an ioctl call
// and goto a label if it fails. For me, this helps me read
// the code easier
#define ioctl_or_goto(label, ...)                                            \
  {                                                                          \
    int __err = (ioctl(__VA_ARGS__));                                        \
    if (__err < 0) {                                                         \
      LOG("%s: ioctl(%s): %s", __FUNCTION__, #__VA_ARGS__, strerror(errno)); \
      goto label;                                                            \
    }                                                                        \
  }

// This is a gcc thing but so is a ton of stuff already, so ah well
typedef unsigned __int128 uint128_t;
typedef signed __int128 int128_t;


// Use the nicer modern keywords you see in Go, Rust, the Linux Kernel etc
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef uint128_t u128;
typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;
typedef int128_t i128;

declare_defer_func(pthread_mutex_unlock, pthread_mutex_t*);

// I like this construct for holding a mutex in a scope
#define pthread_mutex_scope_lock(mutex) \
  pthread_mutex_t* __to_unlock_ ##__LINE__ defer(pthread_mutex_unlock) = mutex; \
  pthread_mutex_lock(mutex); \

declare_defer_func(pthread_rwlock_unlock, pthread_rwlock_t*);

#define pthread_rwlock_scope_rdlock(rwlock) \
  pthread_rwlock_t* __to_unlock_ ##__LINE__ defer(pthread_rwlock_unlock) = rwlock; \
  pthread_rwlock_rdlock(rwlock); \

#define pthread_rwlock_scope_wrlock(rwlock) \
  pthread_rwlock_t* __to_unlock_ ##__LINE__ defer(pthread_rwlock_unlock) = rwlock; \
  pthread_rwlock_wrlock(rwlock); \


