#ifndef __WRITER_H__
#define __WRITER_H__

#include "utils.h"
#include <stdarg.h>
#include <stdio.h>

#define WRITER_STRING 0
#define WRITER_FILE 1

typedef struct {
  int type;  // string or stream
  union {
    FILE *f;  // if type==WRITER_FILE, this should be opened
    struct {
      char *base;     // where to store the string (producer allocates, caller
                      // should deallocate)
      int size, ptr;  // alocated size, writer head position
    } str;
  };
} writer_t;

CONSTRUCTOR(writer_t,int type);
DESTRUCTOR(writer_t);
void out_text(writer_t *w, char *format, ...);
void out_raw(writer_t *w, void *base, int n);

#endif
