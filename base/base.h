#ifndef _BASE_
#define _BASE_

#include <stdlib.h>
#include <stdint.h>
#ifndef __cplusplus
#include <stdbool.h>
#endif
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>

#ifndef typeof
#ifdef _MSC_VER
#define typeof decltype
#else
#define typeof __typeof__
#endif
#endif

#ifdef _MSC_VER
#include <algorithm>
#define min std::min
#define max std::max
#define _thread_local_  __declspec(thread)
#else
#define _thread_local_  __thread
#ifndef max
#define max(a,b) ({ typeof(a) _a = (a); typeof(b) _b = (b); _a > _b ? _a : _b; })
#endif
#ifndef min
#define min(a,b) ({ typeof(a) _a = (a); typeof(b) _b = (b); _a < _b ? _a : _b; })
#endif
#endif

#ifdef __cplusplus
    #define global_construct(f) \
        static void f(void); \
        struct f##_t_ { f##_t_(void) { f(); } }; static f##_t_ f##_; \
        static void f(void)
#elif defined(_MSC_VER)
    #pragma section(".CRT$XCU",read)
    #define global_construct2_(f,p) \
        static void f(void); \
        __declspec(allocate(".CRT$XCU")) void (*f##_)(void) = f; \
        __pragma(comment(linker,"/include:" p #f "_")) \
        static void f(void)
    #ifdef _WIN64
        #define global_construct(f) global_construct2_(f,"")
    #else
        #define global_construct(f) global_construct2_(f,"_")
    #endif
#else
    #define global_construct(f) \
        static void f(void) __attribute__((constructor)); \
        static void f(void)
#endif

#define alloc(T) ((T *)alloc_bytes(sizeof(T)))

struct _base_Base;
typedef void *(*Method)();
typedef void *(*InitMethod)(struct _base_Base *);
typedef void (*BaseMethod)(struct _base_Base *);
typedef void *(*Setter)(struct _base_Base *, void *);
typedef float (*Setter_float)(struct _base_Base *, float);
typedef double (*Setter_double)(struct _base_Base *, double);
typedef char (*Setter_char)(struct _base_Base *, char);
typedef short (*Setter_short)(struct _base_Base *, short);
typedef int (*Setter_int)(struct _base_Base *, int);
typedef long (*Setter_long)(struct _base_Base *, long);
typedef struct _base_Base * (*Setter_object)(struct _base_Base *, struct _base_Base *);
typedef void *(*Getter)(struct _base_Base *);
typedef bool (*ModuleLoadMethod)();

#define CLASS_FLAG_ASSEMBLED   1
#define CLASS_FLAG_PREINIT     2
#define CLASS_FLAG_INIT        4
#define CLASS_FLAG_NO_INIT     8

#define null NULL
#ifdef __cplusplus
#define EXPORT extern "C"
#else
#define EXPORT extern
#endif

EXPORT void module_loader_continue(ModuleLoadMethod ml_add);
EXPORT void *alloc_bytes(size_t count);

static inline struct _base_Base *set_float(struct _base_Base *obj, Setter_float m, float value) {
    m(obj, value);
    return obj;
}

static inline struct _base_Base *set_double(struct _base_Base *obj, Setter_double m, double value) {
    m(obj, value);
    return obj;
}

static inline struct _base_Base *set_char(struct _base_Base *obj, Setter_char m, char value) {
    m(obj, value);
    return obj;
}

static inline struct _base_Base *set_short(struct _base_Base *obj, Setter_short m, short value) {
    m(obj, value);
    return obj;
}

static inline struct _base_Base *set_long(struct _base_Base *obj, Setter_long m, long value) {
    m(obj, value);
    return obj;
}

static inline struct _base_Base *set_int(struct _base_Base *obj, Setter_int m, int value) {
    m(obj, value);
    return obj;
}

static inline struct _base_Base *set_arb(struct _base_Base *obj, Setter m, void *value) {
    m(obj, value);
    return obj;
}

static inline struct _base_Base *set_object(struct _base_Base *obj, Setter_object m, struct _base_Base *value) {
    m(obj, value);
    return obj;
}

#endif