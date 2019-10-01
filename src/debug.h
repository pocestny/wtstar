#ifndef __DEBUG_H__
#define __DEBUG_H__

#include <ast.h>
#include <writer.h>
#include <utils.h>

#define MAP_SENTINEL 0xfffffffeU


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
  char *name;
  uint32_t n_members,*member_types;
  char **member_names;
} type_info_t;
  
int populate_type_info(type_info_t *t,const uint8_t*in, int *pos, const int len);
void clear_type_info(type_info_t *t);

typedef struct {
  char *name;
  uint32_t type,num_dim,from_code,addr;
} variable_info_t;

typedef struct {
  uint32_t parent, n_vars;
  variable_info_t *vars;
} scope_info_t;


typedef struct {
  char **files;
  uint32_t n_files;

  char **fn_names;
  uint32_t *fn_items; // reference to items
  uint32_t n_fn;

  item_info_t *items;
  uint32_t n_items;

  code_map_t *source_items_map;

  uint32_t n_types;
  type_info_t *types;

  code_map_t *scope_map;
  uint32_t n_scope_map;
  
  uint32_t n_scopes;
  scope_info_t *scopes;


} debug_info_t;

CONSTRUCTOR(debug_info_t,const uint8_t *in, int *pos, const int len);
DESTRUCTOR(debug_info_t);

void emit_debug_section(writer_t *out, ast_t *ast, int _code_size);

#endif
