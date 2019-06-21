#include "ast.h"
#include <stdlib.h>
#include <string.h>

CONSTRUCTOR(static_type_t, char *name) {
  ALLOC_VAR(r, static_type_t);
  r->name = strdup(name);
  r->size = 0;
  r->members = NULL;
  return r;
}

DESTRUCTOR(static_type_t) {
  if (r == NULL) return;
  free(r->name);
  static_type_member_t_delete(r->members);
}

CONSTRUCTOR(static_type_member_t, char *name, static_type_t *type) {
  ALLOC_VAR(r, static_type_member_t);
  r->name = strdup(name);
  r->type = type;
  r->parent = NULL;
  r->offset = 0;
  r->next = NULL;
  return r;
}

DESTRUCTOR(static_type_member_t) {
  if (r == NULL) return;
  free(r->name);
  list_for(m, static_type_member_t, r->next) {
    free(m->name);
    free(m);
  }
  list_for_end free(r);
}

static_type_member_t * static_type_member_find(static_type_member_t *list,char *name) {
  for (; list && strcmp(list->name, name); list = list->next)
    ;
  return list;
}


CONSTRUCTOR(scope_t) {
  ALLOC_VAR(r, scope_t)
  r->parent = NULL;
  r->items = NULL;
  return r;
}

DESTRUCTOR(scope_t) {
  if (r == NULL) return;
  ast_node_t_delete(r->items);
  free(r);
}

CONSTRUCTOR(ast_node_t, YYLTYPE *iloc, int node_type, ...) {
  ALLOC_VAR(r, ast_node_t)
  r->next = NULL;

  va_list args;
  va_start(args, node_type);

  if (iloc) {
    r->loc.fl = iloc->fl;
    r->loc.ll = iloc->ll;
    r->loc.fc = iloc->fc;
    r->loc.lc = iloc->lc;
    r->loc.fn = iloc->fn;
    r->loc.ln = iloc->ln;
  }
  r->node_type = node_type;
  switch (node_type) {
    case AST_NODE_STATIC_TYPE:
      r->val.t = static_type_t_new(va_arg(args, char *));
      break;
  };

  va_end(args);
  return r;
}

DESTRUCTOR(ast_node_t) {
  if (r == NULL) return;
}

char *ast_node_name(ast_node_t *n) {
  if (!n) return NULL;
  switch (n->node_type) {
    case AST_NODE_STATIC_TYPE:
      return n->val.t->name;
    case AST_NODE_STATIC_TYPE_MEMBER:
      return n->val.tm->name;
    case AST_NODE_VARIABLE:
      return n->val.v->name;
    case AST_NODE_ARRAY:
      return n->val.a->name;
    case AST_NODE_SCOPE:
      return NULL;
    case AST_NODE_FUNCTION:
      return n->val.f->name;
    case AST_NODE_EXPRESSION:
      return NULL;
  }
  return NULL;
}

ast_node_t *ast_node_find(ast_node_t *n, char *name) {
  for (; n && strcmp(ast_node_name(n), name); n = n->next) 
    ;
  return n;
}

CONSTRUCTOR(ast_t) {
  ALLOC_VAR(r, ast_t);
  r->types = NULL;
  r->functions = NULL;
  r->root_scope = scope_t_new();
  r->error_occured = 0;
  return r;
}

DESTRUCTOR(ast_t) {
  if (r == NULL) return;
}

int ident_role(ast_t *ast, scope_t *scope, char *ident) { return 0; }

void emit_code(ast_t *ast, writer_t *out, writer_t *log) {}
