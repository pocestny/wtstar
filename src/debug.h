#ifndef __DEBUG_H__
#define __DEBUG_H__

#include <ast.h>
#include <writer.h>
#include <utils.h>

typedef struct {
  uint32_t fileid, fl, fc, ll, lc;
} item_info_t;

typedef struct {
  uint32_t *bp,n;
  int32_t  *val;
} code_map_t;

CONSTRUCTOR(code_map_t,const uint8_t*in, int *pos, const int len);
DESTRUCTOR(code_map_t);
int code_map_find(code_map_t *m, uint32_t pos);



typedef struct {
  char **files;
  uint32_t n_files;

  char **fn_names;
  uint32_t n_fn;

  item_info_t *items;
  uint32_t n_items;

  code_map_t *source_items_map;

} debug_info_t;

CONSTRUCTOR(debug_info_t,const uint8_t *in, int *pos, const int len);
DESTRUCTOR(debug_info_t);

void emit_debug_section(writer_t *out, ast_t *ast, int _code_size);

#endif
