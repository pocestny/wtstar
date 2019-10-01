/**
 * @file writer.h
 * @brief Unified way to write text/binary to string/file
 */
#ifndef __WRITER_H__
#define __WRITER_H__

#include <stdarg.h>
#include <stdio.h>

#include <utils.h>

//! where to direct the output
typedef enum { WRITER_STRING = 0, WRITER_FILE } writer_type_t;

/**
 * @brief writer structure.
 *
 * The writer writes either to a string or to a file. For file-based writer,
 * an already opened file should be assigned to `writer.f` before use.
 * The destructor closes the file.
 *
 * A string-based writer allocates the string in constructor, and reallocates
 * as the output grows. The destructor frees the allocated string.
 *
 */
typedef struct {
  writer_type_t type;  //!< string or stream
  union {
    FILE *f;  //!< if type==WRITER_FILE, this file should be opened
    struct {
      char *base;  //!< where to store the string
      int size,    //!< allocated size
          ptr;     //!< writer head position
    } str;
  };
} writer_t;

//! Constructor
CONSTRUCTOR(writer_t, int type);

//! Destructor
DESTRUCTOR(writer_t);

//! Append `printf`-like formatted text to th writer
void out_text(writer_t *w, const char *format, ...);

/**
 * @brief Append `vprintf`-like string to the writer.
 * @note Needs also to know the length of the printed message. Use the macro
 * #get_printed_length from utils.h to find it.
 */
void out_vtext(writer_t *w, int len, const char *format, va_list args);

//! write `n` bytes from a buffer to the writer 
void out_raw(writer_t *w, void *base, int n);

#endif
