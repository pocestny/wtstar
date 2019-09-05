#ifndef __READER_H__
#define __READER_H__

#include <stdio.h>
#include <string.h>

#include <utils.h>

#define READER_STRING 0
#define READER_FILE 1

typedef struct {
  int type;  // string or stream
  union {
    FILE *f;  // if type==READER_FILE, this should be opened
    struct {
      char *base,  // the input string (caller should allocate/free)
          *pos;    // reading position
    } str;
  };
} reader_t;

CONSTRUCTOR(reader_t, int type, ...);
DESTRUCTOR(reader_t);

void _in_text_internal_(reader_t *r, const char *format, ...);

#define in_text(reader, format, ...)                                    \
  {                                                                     \
    int _n_internal_;                                                   \
    char *buf = (char *)malloc(strlen(format) + 100);                   \
    buf[0] = 0;                                                         \
    strcat(buf, format);                                                \
    strcat(buf, "%n");                                                  \
    _in_text_internal_(reader, buf, __VA_ARGS__, &_n_internal_);        \
    if (reader->type == READER_STRING) reader->str.pos += _n_internal_; \
    free(buf);                                                          \
  }

#endif
