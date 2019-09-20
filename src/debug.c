#include <code.h>
#include <debug.h>
#include <hash.h>

#include <errors.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define GET(type, var, b)                                      \
  {                                                            \
    if (*pos + (b) > len) {                                    \
      throw("corrupted debug section (cannot read code map)"); \
      code_map_t_delete(r);                                    \
      return NULL;                                             \
    }                                                          \
    var = *((type *)(in + (*pos)));                            \
    (*pos) += b;                                               \
  }

CONSTRUCTOR(code_map_t, const uint8_t *in, int *pos, const int len) {
  ALLOC_VAR(r, code_map_t);
  r->bp = NULL;
  r->val = NULL;
  r->n = 0;
  GET(uint32_t, r->n, 4);
  r->bp = malloc(r->n * 4);
  r->val = malloc(r->n * 4);
  for (int i = 0; i < r->n; i++) {
    GET(uint32_t, r->bp[i], 4);
    GET(int32_t, r->val[i], 4);
  }
  return r;
}
#undef GET

DESTRUCTOR(code_map_t) {
  if (r->bp) free(r->bp);
  if (r->val) free(r->val);
  if (r == NULL) return;
}


int code_map_find(code_map_t *m, uint32_t pos) {
  if (m->n==0) return -1;
  if (pos>=m->bp[m->n-1]) return m->n-1;
  if (pos<m->bp[0]) return -1;
  int l=0,r=m->n-1;
  while(1) {
    if (l==r) return l;
    if (l==r-1) {
      if (pos==m->bp[r]) return r;
      return l;
    }
    int a=(l+r)/2;
    if (pos>=m->bp[a]) l=a;
    else r=a;
  }
  return -1;
}


#define GET(type, var, b)                                        \
  {                                                              \
    if (*pos + (b) > len) {                                      \
      throw("corrupted debug section (cannot read debug info)"); \
      debug_info_t_delete(r);                                    \
      return NULL;                                               \
    }                                                            \
    var = *((type *)(in + (*pos)));                              \
    (*pos) += b;                                                 \
  }

CONSTRUCTOR(debug_info_t, const uint8_t *in, int *pos, const int len) {
  ALLOC_VAR(r, debug_info_t);

  r->files = NULL;
  r->n_files = 0;
  r->fn_names = NULL;
  r->n_fn = 0;
  r->items = NULL;
  r->n_items = 0;
  r->source_items_map = NULL;

  GET(uint32_t, r->n_files, 4);
  r->files = malloc(r->n_files * sizeof(char *));
  for (int i = 0; i < r->n_files; i++) r->files[i] = NULL;
  for (int i = 0; i < r->n_files; i++) {
    r->files[i] = strdup((char *)(in + (*pos)));
    while (in[*pos] && *pos <= len) (*pos)++;
    (*pos)++;
    if (*pos > len) {
      throw("corrupted debug section (cannot read files) ");
      debug_info_t_delete(r);
      return NULL;
    }
  }

  GET(uint32_t, r->n_fn, 4);
  r->fn_names = malloc(r->n_fn * sizeof(char *));
  for (int i = 0; i < r->n_fn; i++) r->fn_names[i] = NULL;
  for (int i = 0; i < r->n_fn; i++) {
    r->fn_names[i] = strdup((char *)(in + (*pos)));
    while (in[*pos] && *pos <= len) (*pos)++;
    (*pos)++;
    if (*pos > len) {
      throw("corrupted debug section (cannot read function names)");
      debug_info_t_delete(r);
      return NULL;
    }
  }

  GET(uint32_t, r->n_items, 4);
  r->items = malloc(r->n_items * sizeof(debug_info_t));
  for (int i = 0; i < r->n_items; i++) {
    GET(uint32_t, r->items[i].fileid, 4);
    GET(uint32_t, r->items[i].fl, 4);
    GET(uint32_t, r->items[i].fc, 4);
    GET(uint32_t, r->items[i].ll, 4);
    GET(uint32_t, r->items[i].lc, 4);
  }

  r->source_items_map = code_map_t_new(in,pos,len);
  if (!r->source_items_map) {
    debug_info_t_delete(r);
    return NULL;
  }

  return r;
}
#undef GET

DESTRUCTOR(debug_info_t) {
  if (r->files) {
    for (int i = 0; i < r->n_files; i++)
      if (r->files[i]) free(r->files[i]);
    free(r->files);
  }
  if (r->fn_names) {
    for (int i = 0; i < r->n_fn; i++)
      if (r->fn_names[i]) free(r->fn_names[i]);
    free(r->fn_names);
  }
  if (r->items) free(r->items);
  if (r->source_items_map) code_map_t_delete(r->source_items_map);
  free(r);
}

static const char **files =
    NULL;  // an array of pointers to allcated names (in driver)
static uint32_t n_files = 0;

static int *code_source = NULL;  // id of the ast_node that generated this code
static int code_size = 0;

static ast_node_t **items = NULL;  // nodes that should be included in map
static uint32_t n_items;

static void clear_globals() {
  if (files) free(files);
  files = NULL;
  n_files = 0;
  if (code_source) free(code_source);
  code_source = NULL;
  code_size = 0;
  if (items) free(items);
  items = NULL;
  n_items = 0;
}

int find_file(const char *fname) {
  for (int i = 0; i < n_files; i++)
    if (!strcmp(fname, files[i])) return i;
  return -1;
}

static void gather_scope(scope_t *sc);

static void gather_node(ast_node_t *nd) {
  printf("id=%d type=%d code_owner=[%d-%d] ", nd->id, nd->node_type,
         nd->code_from, nd->code_to);
  if (nd->loc.fn == NULL)
    printf("no location info\n");
  else
    printf("%s:%d.%d\n", nd->loc.fn, nd->loc.fl, nd->loc.fc);

  switch (nd->node_type) {
    case AST_NODE_STATEMENT:
      switch (nd->val.s->variant) {
        case STMT_COND:
        case STMT_WHILE:
        case STMT_DO:
          gather_node(nd->val.s->par[1]);
          break;
        case STMT_FOR:
        case STMT_PARDO:
          gather_node(nd->val.s->par[0]);
          break;
      }

      break;
    case AST_NODE_SCOPE:
      gather_scope(nd->val.sc);
      break;
  }
  if (nd->loc.fn && nd->code_from >= 0 && nd->code_to >= 0) {
    if (find_file(nd->loc.fn) == -1) {
      n_files++;
      files = realloc(files, n_files * sizeof(char *));
      files[n_files - 1] = nd->loc.fn;
    }
    int it = -1;
    for (int i = 0; i < n_items; i++)
      if (items[i] == nd) {
        it = i;
        break;
      }
    if (it == -1) {
      n_items++;
      items = realloc(items, n_items * sizeof(ast_node_t *));
      items[n_items - 1] = nd;
      it = n_items - 1;
    }
    for (int i = nd->code_from; i <= nd->code_to; i++)
      if (code_source[i] == -1) code_source[i] = it;
  }
}

static void gather_scope(scope_t *sc) {
  if (!sc) return;
  for (ast_node_t *it = sc->items; it; it = it->next) gather_node(it);
}

void emit_debug_section(writer_t *out, ast_t *ast, int _code_size) {
  clear_globals();
  uint8_t section = SECTION_DEBUG;
  out_raw(out, &section, 1);

  code_size = _code_size;
  code_source = (int *)malloc(code_size * sizeof(int));
  for (int i = 0; i < code_size; i++) code_source[i] = -1;

  for (ast_node_t *fn = ast->functions; fn; fn = fn->next)
    gather_scope(fn->val.f->root_scope);
  gather_scope(ast->root_scope);

  for (ast_node_t *fn = ast->functions; fn; fn = fn->next)
    if (fn->val.f->root_scope) {
      int it = -1;
      for (int i = 0; i < n_items; i++)
        if (items[i] == fn) {
          it = i;
          break;
        }
      if (it == -1) {
        n_items++;
        items = realloc(items, n_items * sizeof(ast_node_t *));
        items[n_items - 1] = fn;
        it = n_items - 1;
      }
      for (int i = fn->code_from; i <= fn->code_to; i++)
        if (code_source[i] == -1) code_source[i] = it;
    }

  // write file names
  out_raw(out, &n_files, 4);
  for (int i = 0; i < n_files; i++) {
    out_raw(out, (char *)files[i], 1 + strlen(files[i]));
  }

  {
    // write function names
    uint32_t n = 0;
    for (ast_node_t *fn = ast->functions; fn; fn = fn->next)
      if (fn->val.f->root_scope) n++;
    out_raw(out, &n, 4);
    for (ast_node_t *fn = ast->functions; fn; fn = fn->next)
      if (fn->val.f->root_scope) {
        out_raw(out, (char *)fn->val.f->name, 1 + strlen(fn->val.f->name));
      }
  }

  // write node info
  out_raw(out, &n_items, 4);
  for (int i = 0; i < n_items; i++) {
    uint32_t fileid = (uint32_t)find_file(items[i]->loc.fn);
    out_raw(out, &fileid, 4);
    out_raw(out, &(items[i]->loc.fl), 4);
    out_raw(out, &(items[i]->loc.fc), 4);
    out_raw(out, &(items[i]->loc.ll), 4);
    out_raw(out, &(items[i]->loc.lc), 4);
  }

  {
    uint32_t code_map_size = 0;
    for (int i = 1; i < code_size; i++)
      if (code_source[i - 1] != code_source[i]) code_map_size++;
    out_raw(out, &code_map_size, 4);
    for (int i = 1; i < code_size; i++)
      if (code_source[i - 1] != code_source[i]) {
        out_raw(out, &i, 4);
        out_raw(out, &code_source[i], 4);
        printf("(%d %d) ",i,code_source[i]);
      }
    printf("\n");
  }
}
