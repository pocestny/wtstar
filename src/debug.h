#ifndef __DEBUG_H__
#define __DEBUG_H__

#include <ast.h>
#include <writer.h>

typedef struct {
  uint32_t fileid, fl, fc, ll, lc;
} item_info_t;

typedef struct {
  uint32_t *i,*val,n;
} code_map_t;

typedef struct {
  char **files;
  uint32_t n_files;

  char **fn_names;
  uint32_t n_fn;

  item_info_t *items;
  uint32_t n_items;

} debug_info_t;

CONSTRUCTOR(debug_info_t,uint8_t *in, int *pos, int len);
DESTRUCTOR(debug_info_t);

void emit_debug_section(writer_t *out, ast_t *ast, int _code_size);

#endif
