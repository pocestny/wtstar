#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include <reader.h>

/* implement  reader  */

reader_t *reader_t_new(int type, ... ) {
  ALLOC_VAR(r, reader_t)
  r->type = type;
  va_list args;
  va_start(args,type);
  if (r->type == READER_STRING) {
    r->str.base = va_arg(args,char*);
    r->str.pos = r->str.base;
  } else {
    r->f = va_arg(args,FILE*);
  }
  va_end(args);
  return r;
}

DESTRUCTOR(reader_t) {
  if (r == NULL) return;
  free(r);
}


void _in_text_internal_(reader_t *r, const char *format, ...) {
 va_list args;
 va_start(args,format);
 if (r->type == READER_STRING) {
   vsscanf(r->str.pos,format,args);
 } else {
   vfscanf(r->f,format,args);
 }
 va_end(args);
}

