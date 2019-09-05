#ifndef __ERRORS_H__
#define __ERRORS_H__

#include <stdarg.h>
#include <writer.h>

typedef struct {
  writer_t *msg;
} error_t;

CONSTRUCTOR(error_t);
DESTRUCTOR(error_t);

void append_error_msg(error_t *err, const char *format,...);
void append_error_vmsg(error_t *err, int len, const char *format, va_list args);

void register_error_handler( void(*handler)(error_t*) );
void emit_error(error_t *err);

void clear_errors();
void delete_errors();

int errnum();
error_t * get_error(int i);
const char * get_error_msg(int i);
#endif
