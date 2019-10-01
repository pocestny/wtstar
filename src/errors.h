/**
 * @file errors.h
 *
 * @brief Unified error handling routines.
 *
 * Provide a type #error_t with the description of an error. Content can be added
 * using #append_error_msg and #append_error_vmsg. There is a static error log
 * in errors.c. Calling #emit_error appends the error to the log, and executes
 * a handler, if some was registered using #register_error_handler.
 *
 */
#ifndef __ERRORS_H__
#define __ERRORS_H__

#include <stdarg.h>
#include <writer.h>

//! convenience macro to create an error, insert a message, and emit
#define throw(...){error_t *err = error_t_new(); \
                   append_error_msg(err, __VA_ARGS__); emit_error(err); }

/**
 * @brief an object for error handling
 * @todo add several parameters (i.e. warning/severity...)
 */
typedef struct {
  writer_t *msg;
} error_t;

//! Allocate an error_t struct and return the pointer.
CONSTRUCTOR(error_t);

//! Free the error_t object.
DESTRUCTOR(error_t);

//! Apppend `printf`-like formatted string to the error message.
void append_error_msg(error_t *err, const char *format, ...);

/**
 * @brief Append `vprintf`-like string to the error message.
 * @note Needs also to know the length of the printed message. Use the macro
 * #get_printed_length from utils.h to find it.
 */
void append_error_vmsg(error_t *err, int len, const char *format, va_list args);

//! Register a function called within #emit_error.
void register_error_handler(void (*handler)(error_t *));
//! Insert error to the internal log, and call error_handler, if defined.
void emit_error(error_t *err);

//! Clear the internal log.
void clear_errors();
//! Clear the internl log and deallocate all memory.
void delete_errors();

//! Return number of errors currently stored in the internal log.
int errnum();
/**
 * @brief Return the i-th error from the log.
 * The error is still owned by the internal log and the caller should not
 * deallocate this.
 */
error_t *get_error(int i);
//! Return the message string of the i-th error from the log.
const char *get_error_msg(int i);
#endif
