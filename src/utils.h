/**
 * @file utils.h
 * @brief various utilities used in several places
 */
#ifndef __UTILS_H__
#define __UTILS_H__
#include <inttypes.h>

//! pointer of given type at given address
#define ptr(pntr, type) ((type *)(pntr))
//! lvalue of given type at given address
#define lval(pntr, type) (*ptr(pntr, type))

//! allocate a variable of given type
#define ALLOC_VAR(var, type) type *var = (type *)malloc(sizeof(type));

//! create constructor
#define CONSTRUCTOR(name, ...) name *name##_new(__VA_ARGS__)
//! create destructor
#define DESTRUCTOR(name) void name##_delete(name *r)

//! append to a list
#define append(type, list, value)                                   \
  if (value) {                                                      \
    if (!(*(list)))                                                 \
      *(list) = (value);                                            \
    else {                                                          \
      type *__tmp__;                                                \
      for (__tmp__ = *list; __tmp__->next; __tmp__ = __tmp__->next) \
        ;                                                           \
      __tmp__->next = (value);                                      \
    }                                                               \
  }

//! iterate a list
#define list_for(var, type, list)        \
  for (type *__tmp__ = list; __tmp__;) { \
    type *var = __tmp__;                 \
    __tmp__ = __tmp__->next;

#define list_for_end }

#define YYLTYPE YYLTYPE
//! location type structure for flex/bison
typedef struct YYLTYPE {
  uint32_t fl, fc, ll, lc;
  const char *fn, *ln;
} YYLTYPE;

#ifndef YYLLOC_DEFAULT
#define YYLLOC_DEFAULT(Current, Rhs, N)                  \
  do                                                     \
    if (N) {                                             \
      (Current).fl = YYRHSLOC(Rhs, 1).fl;                \
      (Current).fc = YYRHSLOC(Rhs, 1).fc;                \
      (Current).ll = YYRHSLOC(Rhs, N).ll;                \
      (Current).lc = YYRHSLOC(Rhs, N).lc;                \
      (Current).fn = YYRHSLOC(Rhs, N).fn;                \
      (Current).ln = YYRHSLOC(Rhs, N).ln;                \
    } else {                                             \
      (Current).fl = (Current).ll = YYRHSLOC(Rhs, 0).ll; \
      (Current).fc = (Current).lc = YYRHSLOC(Rhs, 0).lc; \
      (Current).fn = (Current).ln = YYRHSLOC(Rhs, 0).ln; \
    }                                                    \
  while (0)
#endif

/**
 * @brief Macro to get the length of the formatted string. 
 * Expects to be called from somewhere where the `format` parameter
 * is followed by a `va_list`.
 * The result is stored in `len`
 */
#define get_printed_length(format, len)     \
  {                                         \
    va_list _args;                          \
    va_start(_args, format);                \
    char *buf;                              \
    len = vsnprintf(buf, 0, format, _args); \
    va_end(_args);                          \
  }

#endif
