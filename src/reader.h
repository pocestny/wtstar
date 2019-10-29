/**
 * @file: reader.h
 * @brief Unified way to read text/binary from string/file
 */
#ifndef __READER_H__
#define __READER_H__

#include <stdio.h>
#include <string.h>

#include <utils.h>

//! where the input is from
typedef enum { READER_STRING = 0, READER_FILE } reader_type_t;

//! the reader structure
typedef struct {
  int type;  //!< string or stream
  union {
    FILE *f;  //!< if type==READER_FILE, this should be opened
    struct {
      char *base,  //!< the input string (caller should allocate/free)
          *pos;    //!< reading position
    } str;
  };
} reader_t;

//! constructor
CONSTRUCTOR(reader_t, int type, ...);
//! destructor
DESTRUCTOR(reader_t);

//! return one symbol back
void in_ungetc(reader_t *r, const char c);
//! internal used in macro in_text
int _in_text_internal_(reader_t *r, const char *format, ...);
//! read text
#define in_text(reader, result, format, ...)                              \
  {                                                                       \
    int _n_internal_;                                                     \
    char *buf = (char *)malloc(strlen(format) + 100);                     \
    buf[0] = 0;                                                           \
    strcat(buf, format);                                                  \
    strcat(buf, "%n");                                                    \
    result = _in_text_internal_(reader, buf, __VA_ARGS__, &_n_internal_); \
    if (reader->type == READER_STRING) reader->str.pos += _n_internal_;   \
    free(buf);                                                            \
  }

#endif
