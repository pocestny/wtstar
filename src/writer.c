#include <stdlib.h>
#include <string.h>

#include <writer.h>

/* implement  writer  */
#define WRITER_BASE_STRING_SIZE 10

writer_t *writer_t_new(int type) {
  ALLOC_VAR(r, writer_t)
  r->type = type;
  if (r->type == WRITER_STRING) {
    r->str.base = (char *)malloc(WRITER_BASE_STRING_SIZE);
    r->str.size = WRITER_BASE_STRING_SIZE;
    *(r->str.base) = 0;
    r->str.ptr = 0;
  }
  return r;
}

DESTRUCTOR(writer_t) {
  if (r == NULL) return;
  if (r->type == WRITER_FILE)
    fclose(r->f);
  else
    free(r->str.base);
  free(r);
}

void out_vtext(writer_t *w, int len, const char *format, va_list args) {
  if (w->type == WRITER_STRING) {
    while (len  >= w->str.size - w->str.ptr  ) {
      w->str.base = (char *)realloc(w->str.base, 2 * w->str.size );
      w->str.size *= 2;
    }
    w->str.ptr+=vsprintf(w->str.base + w->str.ptr, format, args);
  } else {
    vfprintf(w->f, format, args);
  }
}

void out_text(writer_t *w, const char *format, ...) {
  int n=0;
  va_list args;
  if (w->type == WRITER_STRING) get_printed_length(format,n);
  va_start(args, format);
  out_vtext(w,n,format,args);
  va_end(args);
}

void out_raw(writer_t *w, void *base, int n) {
  if (w->type == WRITER_STRING) {
    while (n >= w->str.size - w->str.ptr) {
      w->str.base = (char *)realloc(w->str.base, 2 * w->str.size);
      w->str.size *= 2;
    }
    memcpy((void *)(w->str.base + w->str.ptr), base, n);
    w->str.ptr += n;
  } else {
    fwrite(base, 1, n, w->f);
  }
}

#undef WRITER_BASE_STRING_SIZE
