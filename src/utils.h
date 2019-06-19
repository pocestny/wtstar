#ifndef __UTILS_H__
#define __UTILS_H__

// lvalue of given type at given address
#define LVAL(ptr, type) *((type *)(ptr))

#define ALLOC_VAR(var, type) type *var = (type *)malloc(sizeof(type));

#define CONSTRUCTOR(name,...) name *name##_new(__VA_ARGS__)
#define DESTRUCTOR(name) void name##_delete(void *data)

#define YYLTYPE YYLTYPE
  typedef struct YYLTYPE
  {
    int fl,fc,ll,lc;
    char *fname;
  } YYLTYPE;


#endif
