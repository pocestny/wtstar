#ifndef __UTILS_H__
#define __UTILS_H__

// lvalue of given type at given address
#define LVAL(ptr, type) *((type *)(ptr))

#define ALLOC_VAR(var, type) type *var = (type *)malloc(sizeof(type));

#define CONSTRUCTOR(name, ...) name *name##_new(__VA_ARGS__)
#define DESTRUCTOR(name) void name##_delete(name *r)

#define append(type, list, value)                   \
  if (value) {                                      \
    if (!(*(list)))                                 \
      *(list) = (value);                            \
    else {                                          \
      type *tmp;                                    \
      for (tmp = *list; tmp->next; tmp = tmp->next) \
        ;                                           \
      tmp->next = (value);                          \
    }                                               \
  }

#define list_for(var, type, list)        \
  for (type *__tmp__ = list; __tmp__;) { \
    type *var = __tmp__;                 \
    __tmp__ = __tmp__->next;

#define list_for_end }

#define YYLTYPE YYLTYPE
typedef struct YYLTYPE {
  int fl, fc, ll, lc;
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

#endif
