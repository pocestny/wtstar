#include "writer.h"
#include <stdlib.h>
#include <string.h>


/* implement  writer  */
#define WRITER_BASE_STRING_SIZE 10

writer_t *writer_t_new(int type) {
  ALLOC_VAR(r, writer_t)
  r->type = type;
  if (r->type == WRITER_STRING) {
    r->str.base = (char *)malloc(WRITER_BASE_STRING_SIZE);
    r->str.size = WRITER_BASE_STRING_SIZE;
    r->str.ptr = 0;
  }
  return r;
}

DESTRUCTOR(writer_t) {
  if (r==NULL) return;
  if (r->type == WRITER_FILE)
    fclose(r->f);
  else
    free(r->str.base);
  free(r);
}

void out_text(writer_t *w, char *format, ...) {
  va_list args;

  if (w->type == WRITER_STRING) {
    va_start(args, format);
    int n = snprintf(w->str.base, 0, format, args);
    va_end(args);

    while (n >= w->str.size - w->str.ptr) {
      w->str.base = (char *)realloc(w->str.base, 2 * w->str.size);
      w->str.size *= 2;
    }
    va_start(args, format);
    vsprintf(w->str.base + w->str.ptr, format, args);
    va_end(args);
    while (*(w->str.base + w->str.ptr)) (w->str.ptr)++;
  } else {
    va_start(args, format);
    vfprintf(w->f, format, args);
    va_end(args);
  }
}

void out_raw(writer_t *w, void *base, int n) {
  if (w->type==WRITER_STRING) {
    while (n >= w->str.size - w->str.ptr) {
      w->str.base = (char *)realloc(w->str.base, 2 * w->str.size);
      w->str.size *= 2;
    }
    memcpy((void *)(w->str.base+w->str.ptr),base,n);
    w->str.ptr+=n;
  } else {
    fwrite(base,1,n,w->f);
  }
}
