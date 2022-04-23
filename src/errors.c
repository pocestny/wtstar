#include <errors.h>
#include <utils.h>

#include <stdlib.h>

static error_t **log = NULL;  // internal error log
static int n_err = 0,         // number of errors stored
    logsize = 0;              // allocated space

static void (*registered_error_handler)(error_t *, void *) = NULL;  // registered error handler

void default_error_handler(error_t *err, void *data) {
  fprintf(stderr, "%s\n", err->msg->str.base);
}

CONSTRUCTOR(error_t) {
  ALLOC_VAR(r, error_t);
  r->msg = writer_t_new(WRITER_STRING);
  return r;
}

DESTRUCTOR(error_t) {
  if (r == NULL) return;
  writer_t_delete(r->msg);
  free(r);
}

void append_error_msg(error_t *err, const char *format, ...) {
  va_list args;
  int n;
  get_printed_length(format, n);
  va_start(args, format);
  out_vtext(err->msg, n, format, args);
  va_end(args);
}

void append_error_vmsg(error_t *err, int len, const char *format,
                       va_list args) {
  out_vtext(err->msg, len, format, args);
}

void register_error_handler(error_handler_t handler) {
  registered_error_handler = handler;
}

void emit_error(error_t *err) {
  emit_error_handle(err, NULL, NULL);
}

void emit_error_handle(error_t *err, error_handler_t error_handler, void *user_data) {
  if (n_err >= logsize) {
    if (logsize < 8)
      logsize = 8;
    else
      logsize *= 2;
    log = (error_t **)realloc(log, logsize * sizeof(error_t *));
  }
  log[n_err++] = err;
  if (error_handler) error_handler(err, user_data);
  else if (registered_error_handler) registered_error_handler(err, user_data);
  // else default_error_handler(err, user_data);
}

void clear_errors() {
  for (int i = 0; i < n_err; i++) error_t_delete(log[i]);
  n_err = 0;
}

void delete_errors() {
  clear_errors();
  free(log);
  log = NULL;
  logsize = 0;
}

int errnum() { return n_err; }
error_t *get_error(int i) { return log[i]; }
const char *get_error_msg(int i) { return log[i]->msg->str.base; }
