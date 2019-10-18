#include <code.h>
#include <debug.h>
#include <hash.h>

#include <errors.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define GET(type, var, b)                                       \
  {                                                             \
    if (*pos + (b) > len) {                                     \
      throw("corrupted debug section (cannot read type info)"); \
      clear_type_info(t);                                       \
      return 0;                                                 \
    }                                                           \
    var = *((type *)(in + (*pos)));                             \
    (*pos) += b;                                                \
  }

int populate_type_info(type_info_t *t, const uint8_t *in, int *pos,
                       const int len) {
  t->n_members = 0;
  t->member_types = NULL;
  t->member_names = NULL;
  t->name = strdup((char *)(in + (*pos)));
  while (in[*pos] && *pos <= len) (*pos)++;
  (*pos)++;
  if (*pos > len) {
    throw("corrupted debug section (cannot read type info) ");
    clear_type_info(t);
    return 0;
  }
  GET(uint32_t, t->n_members, 4);
  t->member_names = malloc(t->n_members * sizeof(char *));
  t->member_types = malloc(t->n_members * 4);
  for (int i = 0; i < t->n_members; i++) {
    t->member_names[i] = strdup((char *)(in + (*pos)));
    while (in[*pos] && *pos <= len) (*pos)++;
    (*pos)++;
    if (*pos > len) {
      throw("corrupted debug section (cannot read type info) ");
      clear_type_info(t);
      return 0;
    }
    GET(uint32_t, t->member_types[i], 4);
  }
  return 1;
}

#undef GET

void clear_type_info(type_info_t *t) {
  if (t->name) free(t->name);
  t->name = NULL;
  for (int i = 0; i < t->n_members; i++) {
    if (t->member_names[i]) free(t->member_names[i]);
    t->member_names[i] = NULL;
  }
  if (t->member_names) free(t->member_names);
  t->member_names = NULL;
  if (t->member_types) free(t->member_types);
  t->member_types = NULL;
}

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
  //printf("code_map_find %d\n",pos);
  if (m->n == 0) return -1;
  if (pos >= m->bp[m->n - 1]) return m->n - 1;
  if (pos < m->bp[0]) return -1;
  int l = 0, r = m->n - 1;
  while (1) {
    if (l == r) return l;
    if (l == r - 1) {
      if (pos == m->bp[r]) return r;
      return l;
    }
    int a = (l + r) / 2;
    if (pos >= m->bp[a])
      l = a;
    else
      r = a;
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
  //printf("debug info constructor\n");
  ALLOC_VAR(r, debug_info_t);

  r->files = NULL;
  r->n_files = 0;
  r->fn_names = NULL;
  r->n_fn = 0;
  r->items = NULL;
  r->n_items = 0;
  r->source_items_map = NULL;
  r->scope_map = NULL;
  r->n_scopes = 0;
  r->scopes = NULL;

  GET(uint32_t, r->n_files, 4);
  //printf("%d files\n",r->n_files);
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
  //printf("%d functions\n",r->n_fn);
  r->fn_names = malloc(r->n_fn * sizeof(char *));
  r->fn_items = malloc(r->n_fn * 4);
  for (int i = 0; i < r->n_fn; i++) r->fn_names[i] = NULL;
  for (int i = 0; i < r->n_fn; i++) {
    GET(uint32_t, r->fn_items[i], 4);
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
  //printf("%d items\n",r->n_items);
  r->items = malloc(r->n_items * sizeof(debug_info_t));
  for (int i = 0; i < r->n_items; i++) {
    GET(uint32_t, r->items[i].fileid, 4);
    GET(uint32_t, r->items[i].fl, 4);
    GET(uint32_t, r->items[i].fc, 4);
    GET(uint32_t, r->items[i].ll, 4);
    GET(uint32_t, r->items[i].lc, 4);
  }

  //printf("source map\n");
  r->source_items_map = code_map_t_new(in, pos, len);
  if (!r->source_items_map) {
    debug_info_t_delete(r);
    return NULL;
  }

  GET(uint32_t, r->n_types, 4);
  //printf("%d types\n",r->n_types);
  r->types = malloc(r->n_types * sizeof(type_info_t));
  for (int i = 0; i < r->n_types; i++) {
    r->types[i].name = NULL;
    r->types[i].n_members = 0;
    r->types[i].member_types = NULL;
    r->types[i].member_names = NULL;
  }
  for (int i = 0; i < r->n_types; i++)
    if (!populate_type_info(&(r->types[i]), in, pos, len)) {
      debug_info_t_delete(r);
      return NULL;
    }

  //printf("scope map\n");
  r->scope_map = code_map_t_new(in, pos, len);
  if (!r->scope_map) {
    debug_info_t_delete(r);
    return NULL;
  }

  GET(uint32_t, r->n_scopes, 4);
  //printf("%d scopes\n",r->n_scopes);
  r->scopes = malloc(r->n_scopes * sizeof(scope_info_t));
  for (int i = 0; i < r->n_scopes; i++) r->scopes[i].n_vars = 0;

  for (int i = 0; i < r->n_scopes; i++) {
    GET(uint32_t, r->scopes[i].parent, 4);
    GET(uint32_t, r->scopes[i].n_vars, 4);
    r->scopes[i].vars = malloc(r->scopes[i].n_vars * sizeof(variable_info_t));
    for (int j = 0; j < r->scopes[i].n_vars; j++)
      r->scopes[i].vars[j].name = NULL;
    for (int j = 0; j < r->scopes[i].n_vars; j++) {
      r->scopes[i].vars[j].name = strdup((char *)(in + (*pos)));
      while (in[*pos] && *pos <= len) (*pos)++;
      (*pos)++;
      if (*pos > len) {
        throw("corrupted debug section (cannot read variables)");
        debug_info_t_delete(r);
        return NULL;
      }
      GET(uint32_t, r->scopes[i].vars[j].type, 4);
      GET(uint32_t, r->scopes[i].vars[j].num_dim, 4);
      GET(uint32_t, r->scopes[i].vars[j].from_code, 4);
      GET(uint32_t, r->scopes[i].vars[j].addr, 4);
    }
  }

  //printf("debug info ready\n");
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
  if (r->fn_items) free(r->fn_items);

  if (r->items) free(r->items);
  if (r->source_items_map) code_map_t_delete(r->source_items_map);
  if (r->scope_map) code_map_t_delete(r->scope_map);

  for (int i = 0; i < r->n_types; i++) clear_type_info(&(r->types[i]));

  for (int s = 0; s < r->n_scopes; s++) {
    for (int v = 0; v < r->scopes[s].n_vars; v++)
      if (r->scopes[s].vars[v].name) free(r->scopes[s].vars[v].name);
    free(r->scopes[s].vars);
  }

  if (r->scopes) free(r->scopes);
  free(r);
}

static const char **files =
    NULL;  // an array of pointers to allcated names (in driver)
static uint32_t n_files = 0;

static int *code_source =
    NULL;  // the index in items of the ast_node that generated this code
static int *code_scope = NULL;
static int code_size = 0;

static ast_node_t **items = NULL;  // nodes that should be included in map
static uint32_t n_items;

static scope_t **scopes = NULL;  // scope map
static uint32_t n_scopes;

static ast_node_t **variables = NULL;
static uint32_t n_variables;

static void clear_globals() {
  if (files) free(files);
  files = NULL;
  n_files = 0;
  if (code_source) free(code_source);
  code_source = NULL;
  if (code_scope) free(code_scope);
  code_scope = NULL;
  code_size = 0;
  if (items) free(items);
  items = NULL;
  n_items = 0;
  if (scopes) free(scopes);
  scopes = NULL;
  n_scopes = 0;
  if (variables) free(variables);
  variables = NULL;
  n_variables = 0;
}

static int find_file(const char *fname) {
  for (int i = 0; i < n_files; i++)
    if (!strcmp(fname, files[i])) return i;
  return -1;
}

static void debug_print(ast_node_t *nd) {
  printf("id=%d type=%d code_owner=[%d-%d] ", nd->id, nd->node_type,
         nd->code_from, nd->code_to);
  if (nd->loc.fn == NULL)
    printf("no location info\n");
  else
    printf("%s:%d.%d\n", nd->loc.fn, nd->loc.fl, nd->loc.fc);
}

static void gather_scope(scope_t *sc);

static void gather_node(ast_node_t *nd) {
  //debug_print(nd);
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
    case AST_NODE_FUNCTION:
      for (ast_node_t *par = nd->val.f->params;par;par=par->next)
        gather_node(par);
      gather_scope(nd->val.f->root_scope);
      break;
  }

  // store item to source map
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
      if (code_source[i] == MAP_SENTINEL) code_source[i] = it;
  }

  // store scope to scope map
  if (nd->node_type == AST_NODE_SCOPE ||
      (nd->node_type == AST_NODE_FUNCTION && nd->val.f->root_scope)) {
    scope_t *sc;
    if (nd->node_type == AST_NODE_SCOPE)
      sc = nd->val.sc;
    else
      sc = nd->val.f->root_scope;

    int it = -1;
    for (int i = 0; i < n_scopes; i++)
      if (scopes[i] == sc) {
        it = i;
        break;
      }
    if (it == -1) {
      n_scopes++;
      scopes = realloc(scopes, n_scopes * sizeof(scope_t *));
      scopes[n_scopes - 1] = sc;
      it = n_scopes - 1;
    }
    if (nd->code_from >= 0)
      for (int i = nd->code_from; i <= nd->code_to; i++)
        if (code_scope[i] == MAP_SENTINEL) code_scope[i] = it;
  }

  // store variables
  if (nd->node_type == AST_NODE_VARIABLE) {
    int it = -1;
    for (int i = 0; i < n_variables; i++)
      if (variables[i] == nd) {
        it = i;
        break;
      }
    if (it == -1) {
      n_variables++;
      variables = realloc(variables, n_variables * sizeof(ast_node_t *));
      variables[n_variables - 1] = nd;
    }
  }
}

static void gather_scope(scope_t *sc) {
  if (!sc) return;
  for (ast_node_t *it = sc->items; it; it = it->next) gather_node(it);
}

static void emit_variable(writer_t *out, ast_t *ast, int j) {
  out_raw(out, variables[j]->val.v->name,
          strlen(variables[j]->val.v->name) + 1);
  uint32_t type;
  for (ast_node_t *t = ast->types; t; t = t->next)
    if (t->val.t == variables[j]->val.v->base_type) {
      type = t->val.t->id;
      break;
    }

  out_raw(out, &type, 4);
  out_raw(out, &(variables[j]->val.v->num_dim), 4);
  
  if (variables[j]->code_from<0) variables[j]->code_from=0;
  out_raw(out, &(variables[j]->code_from), 4);
  out_raw(out, &(variables[j]->val.v->addr), 4);
}

void emit_debug_section(writer_t *out, ast_t *ast, int _code_size) {
  clear_globals();
  n_scopes=1;
  scopes=malloc(sizeof(scope_t*));
  scopes[0]=ast->root_scope;
  uint8_t section = SECTION_DEBUG;
  out_raw(out, &section, 1);

  code_size = _code_size;
  code_source = (int *)malloc(code_size * sizeof(int));
  code_scope = (int *)malloc(code_size * sizeof(int));
  for (int i = 0; i < code_size; i++) code_source[i] = MAP_SENTINEL;
  for (int i = 0; i < code_size; i++) code_scope[i] = MAP_SENTINEL;

  for (ast_node_t *fn = ast->functions; fn; fn = fn->next) gather_node(fn);
  gather_scope(ast->root_scope);

  // write file names
  out_raw(out, &n_files, 4);
  for (int i = 0; i < n_files; i++) {
    out_raw(out, (char *)files[i], 1 + strlen(files[i]));
  }

  {
    // write function info
    uint32_t n = 0;
    for (ast_node_t *fn = ast->functions; fn; fn = fn->next)
      if (fn->val.f->root_scope) n++;
    out_raw(out, &n, 4);
    for (ast_node_t *fn = ast->functions; fn; fn = fn->next)
      if (fn->val.f->root_scope) {
        uint32_t i;
        for (i = 0; i < n_items; i++)
          if (items[i] == fn) break;
        out_raw(out, &i, 4);
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
    // write source map
    uint32_t code_map_size = 0;
    for (int i = 1; i < code_size; i++)
      if (code_source[i - 1] != code_source[i]) code_map_size++;
    out_raw(out, &code_map_size, 4);
    for (int i = 1; i < code_size; i++)
      if (code_source[i - 1] != code_source[i]) {
        out_raw(out, &i, 4);
        out_raw(out, &code_source[i], 4);

      }
  }

  {
    // write type info
    uint32_t n = 0;
    for (ast_node_t *t = ast->types; t; t = t->next) t->val.t->id = n++;
    out_raw(out, &n, 4);
    //printf("write types: %d\n",n);
    for (ast_node_t *t = ast->types; t; t = t->next) {
      out_raw(out, t->val.t->name, strlen(t->val.t->name) + 1);
      uint32_t nm = 0;
      for (static_type_member_t *m = t->val.t->members; m; m = m->next) nm++;
      out_raw(out, &nm, 4);
      for (static_type_member_t *m = t->val.t->members; m; m = m->next) {
        out_raw(out, m->name, strlen(m->name) + 1);
        out_raw(out, &(m->type->id), 4);
      }
    }
  }

  {
    // write scope map
    uint32_t scope_map_size = 0;
    
    for (int i = 0; i < code_size; i++)
      if (code_scope[i]==MAP_SENTINEL) code_scope[i]=0;

    for (int i = 0; i < code_size; i++)
      if (i==0||code_scope[i - 1] != code_scope[i]) scope_map_size++;
    out_raw(out, &scope_map_size, 4);
    for (int i = 0; i < code_size; i++)
      if (i==0||code_scope[i - 1] != code_scope[i]) {
        out_raw(out, &i, 4);
        out_raw(out, &code_scope[i], 4);
      }
  }

  {
    // write scopes
    out_raw(out, &n_scopes, 4);
    for (int i = 0; i < n_scopes; i++) {
      uint32_t parent = MAP_SENTINEL;
      for (int j = 0; j < n_scopes; j++)
        if (scopes[i]->parent == scopes[j]) {
          parent = j;
          break;
        }
      out_raw(out, &parent, 4);
      uint32_t nv = 0;
      for (int j = 0; j < n_variables; j++)
        if (variables[j]->val.v->scope == scopes[i]) nv++;
      out_raw(out, &nv, 4);
      for (int j = 0; j < n_variables; j++)
        if (variables[j]->val.v->scope == scopes[i]) emit_variable(out, ast, j);
    }
  }
}


