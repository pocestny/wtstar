#include "ast.h"
#include <stdlib.h>
#include <string.h>

/* ----------------------------------------------------------------------------
 * static types
 */

extern ast_node_t *__type__int;
extern ast_node_t *__type__void;

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
  free(r);
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
  list_for_end;
  free(r);
}

static_type_member_t *static_type_member_find(static_type_member_t *list,
                                              char *name) {
  for (; list && strcmp(list->name, name); list = list->next)
    ;
  return list;
}

/* ----------------------------------------------------------------------------
 * variables
 */

CONSTRUCTOR(variable_t, char *name) {
  ALLOC_VAR(r, variable_t)
  r->name = strdup(name);
  r->base_type = NULL;
  r->addr = r->num_dim = 0;
  r->scope = NULL;
  r->io_flag = IO_FLAG_NONE;
  r->initializer = NULL;

  r->active_dims = NULL;
  r->root = r;
  r->orig = r;
  r->ranges = NULL;
  return r;
}

DESTRUCTOR(variable_t) {
  if (r == NULL) return;
  free(r->name);
  expression_t_delete(r->initializer);
  if (r->active_dims) free(r->active_dims);
  ast_node_t_delete(r->ranges);
  free(r);
}

/* ----------------------------------------------------------------------------
 * scopes
 */

CONSTRUCTOR(scope_t) {
  ALLOC_VAR(r, scope_t)
  r->parent = NULL;
  r->items = NULL;
  r->params = NULL;
  return r;
}

DESTRUCTOR(scope_t) {
  if (r == NULL) return;
  ast_node_t_delete(r->items);
  free(r);
}

/* ----------------------------------------------------------------------------
 * functions
 */

CONSTRUCTOR(function_t, char *name) {
  ALLOC_VAR(r, function_t);
  r->name = strdup(name);
  r->out_type = NULL;
  r->params = NULL;
  r->root_scope = NULL;
  return r;
}

DESTRUCTOR(function_t) {
  if (r == NULL) return;
  free(r->name);
  ast_node_t_delete(r->params);
  scope_t_delete(r->root_scope);
}

/* ----------------------------------------------------------------------------
 * expressions
 */

CONSTRUCTOR(inferred_type_t) {
  ALLOC_VAR(r, inferred_type_t);
  r->compound = 0;
  r->type = __type__void->val.t;
  return r;
}

DESTRUCTOR(inferred_type_t) {
  if (r == NULL) return;
  if (r->compound) inferred_type_item_t_delete(r->list);
  free(r);
}

CONSTRUCTOR(inferred_type_item_t, inferred_type_t *tt) {
  ALLOC_VAR(r, inferred_type_item_t);
  r->next = NULL;
  r->type = tt;
  return r;
}

DESTRUCTOR(inferred_type_item_t) {
  if (r == NULL) return;
  if (r->next) inferred_type_item_t_delete(r->next);
  free(r);
}

char *inferred_type_name(inferred_type_t *t) {
  if (t->compound) {
    char *res = strdup("{");
    list_for(ti, inferred_type_item_t, t->list) {
      char *tmp = inferred_type_name(ti->type);
      res = (char *)realloc(res, strlen(res) + strlen(tmp) + 2);
      strcat(res, tmp);
      strcat(res, ",");
      free(tmp);
    }
    list_for_end;
    if (strlen(res) > 1) res[strlen(res) - 1] = 0;
    res = (char *)realloc(res, strlen(res) + 2);
    strcat(res, "}");
    return res;
  } else
    return strdup(t->type->name);
}

inferred_type_t *inferred_type_copy(inferred_type_t *t) {
  if (!t) return NULL;
  inferred_type_t *res = inferred_type_t_new();
  if (t->compound) {
    res->compound = 1;
    list_for(ti, inferred_type_item_t, t->list) {
      append(inferred_type_item_t, &res->list,
             inferred_type_item_t_new(inferred_type_copy(ti->type)));
    }
    list_for_end;
  } else
    res->type = t->type;
  return res;
}

inferred_type_t *inferred_type_append(inferred_type_t *dst,
                                      inferred_type_t *src) {
  if (!dst) return src;

  if (!dst->compound) {
    inferred_type_t *t = inferred_type_t_new();
    t->type = dst->type;
    dst->compound = 1;
    dst->list = inferred_type_item_t_new(t);
  }

  if (!src->compound) {
    inferred_type_t *t = inferred_type_t_new();
    t->type = src->type;
    src->compound = 1;
    src->list = inferred_type_item_t_new(t);
  }

  append(inferred_type_item_t, &(dst->list), src->list);
  free(src);
  return dst;
}

CONSTRUCTOR(expression_t, int variant) {
  ALLOC_VAR(r, expression_t);
  r->variant = variant;
  r->type = inferred_type_t_new();

  switch (variant) {
    case EXPR_EMPTY:
    case EXPR_LITERAL:
      r->val.l = NULL;
      break;
    case EXPR_INITIALIZER:
      r->val.i = NULL;
      break;
    case EXPR_CALL: {
      ALLOC_VAR(v, expr_function_t);
      v->fn = NULL;
      v->params = NULL;
      r->val.f = v;
    } break;
    case EXPR_ARRAY_ELEMENT:
    case EXPR_VAR_NAME:
    case EXPR_IMPLICIT_ALIAS:
    case EXPR_SIZEOF: {
      ALLOC_VAR(v, expr_variable_t);
      v->var = NULL;
      v->params = NULL;
      r->val.v = v;
    } break;
    case EXPR_POSTFIX:
    case EXPR_PREFIX:
    case EXPR_BINARY: {
      ALLOC_VAR(v, expr_oper_t);
      v->first = v->second = NULL;
      r->val.o = v;
    } break;
    case EXPR_CAST: {
      ALLOC_VAR(v, expr_cast_t);
      v->type = NULL;
      v->ex = NULL;
      r->val.c = v;
    } break;
    case EXPR_SPECIFIER: {
      ALLOC_VAR(v, expr_specif_t);
      v->memb = NULL;
      v->ex = NULL;
      r->val.s = v;
    } break;
  }
  return r;
}

DESTRUCTOR(expression_t) {
  if (r == NULL) return;
  inferred_type_t_delete(r->type);
  switch (r->variant) {
    case EXPR_EMPTY:
    case EXPR_LITERAL:
      if (r->val.l) free(r->val.l);
      break;
    case EXPR_INITIALIZER:
      ast_node_t_delete(r->val.i);
      break;
    case EXPR_CALL:
      ast_node_t_delete(r->val.f->params);
      break;
    case EXPR_ARRAY_ELEMENT:
    case EXPR_VAR_NAME:
    case EXPR_IMPLICIT_ALIAS:
    case EXPR_SIZEOF:
      ast_node_t_delete(r->val.v->params);
      break;
    case EXPR_POSTFIX:
    case EXPR_PREFIX:
    case EXPR_BINARY:
      expression_t_delete(r->val.o->first);
      expression_t_delete(r->val.o->second);
      break;
    case EXPR_CAST:
      expression_t_delete(r->val.c->ex);
      break;
    case EXPR_SPECIFIER:
      expression_t_delete(r->val.s->ex);
      break;
  }
  free(r);
}

int expr_int(expression_t *e) {
  if (e->type->compound) {
    if (e->type->list == NULL || e->type->list->next != NULL) return 0;
    if (e->type->list->type->compound) return 0;
    if (e->type->list->type->type == __type__int->val.t) return 1;
    return 0;
  } else {
    if (e->type->type == __type__int->val.t) return 1;
    return 0;
  }
}

/* ----------------------------------------------------------------------------
 * statements
 */

CONSTRUCTOR(statement_t, int variant) {
  ALLOC_VAR(r, statement_t);
  for (int i = 0; i < 2; i++) r->par[i] = NULL;
  r->variant = variant;
  return r;
}

DESTRUCTOR(statement_t) {
  if (r == NULL) return;
  for (int i = 0; i < 2; i++)
    if (r->par[i]) ast_node_t_delete(r->par[i]);
}

/* ----------------------------------------------------------------------------
 * AST node
 */

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
  } else {
    r->loc.fl = 0;
    r->loc.ll = 0;
    r->loc.fc = 0;
    r->loc.lc = 0;
    r->loc.fn = NULL;
    r->loc.ln = NULL;
  }

  r->node_type = node_type;
  switch (node_type) {
    case AST_NODE_STATIC_TYPE:
      r->val.t = static_type_t_new(va_arg(args, char *));
      break;
    case AST_NODE_VARIABLE:
      r->val.v = variable_t_new(va_arg(args, char *));
      break;
    case AST_NODE_SCOPE:
      r->val.sc = scope_t_new();
      r->val.sc->parent = va_arg(args, scope_t *);
      break;
    case AST_NODE_FUNCTION:
      r->val.f = function_t_new(va_arg(args, char *));
      break;
    case AST_NODE_EXPRESSION: {
      int ev = va_arg(args, int);
      r->val.e = expression_t_new(ev);
      switch (ev) {
        case EXPR_BINARY: {
          ast_node_t *n1 = va_arg(args, ast_node_t *);
          int op = va_arg(args, int);
          ast_node_t *n2 = va_arg(args, ast_node_t *);
          r->val.e->val.o->first = (n1)?n1->val.e:NULL;
          r->val.e->val.o->second = (n2)?n2->val.e:NULL;
          r->val.e->val.o->oper = op;
          if (n1) free(n1);
          if (n2) free(n2);
        } break;
        case EXPR_CAST: {
          static_type_t *t = va_arg(args, static_type_t *);
          ast_node_t *ex = va_arg(args, ast_node_t *);
          r->val.e->val.c->type = t;
          r->val.e->val.c->ex = (ex)?ex->val.e:NULL;
          if (t) free(t);
          if (ex) free(ex);
        } break;
        case EXPR_PREFIX: {
          int op = va_arg(args, int);
          ast_node_t *ex = va_arg(args, ast_node_t *);
          r->val.e->val.o->first = (ex)?ex->val.e:NULL;
          r->val.e->val.o->oper = op;
          if (ex) free(ex);
        } break;
        case EXPR_POSTFIX: {
          ast_node_t *ex = va_arg(args, ast_node_t *);
          int op = va_arg(args, int);
          r->val.e->val.o->first = (ex)?ex->val.e:NULL;
          r->val.e->val.o->oper = op;
          if(ex) free(ex);
        } break;
      };
    } break;
    case AST_NODE_STATEMENT:
      r->val.s = statement_t_new(va_arg(args, int));
      break;
  };
  va_end(args);
  return r;
}

DESTRUCTOR(ast_node_t) {
  if (r == NULL) return;
  if (r->next) ast_node_t_delete(r->next);
  switch (r->node_type) {
    case AST_NODE_STATIC_TYPE:
      static_type_t_delete(r->val.t);
      break;
    case AST_NODE_VARIABLE:
      variable_t_delete(r->val.v);
      break;
    case AST_NODE_SCOPE:
      scope_t_delete(r->val.sc);
      break;
    case AST_NODE_FUNCTION:
      function_t_delete(r->val.f);
      break;
    case AST_NODE_EXPRESSION:
      expression_t_delete(r->val.e);
      break;
    case AST_NODE_STATEMENT:
      statement_t_delete(r->val.s);
      break;
  };

  free(r);
}

char *ast_node_name(ast_node_t *n) {
  if (!n) return NULL;
  switch (n->node_type) {
    case AST_NODE_STATIC_TYPE:
      return n->val.t->name;
    case AST_NODE_VARIABLE:
      return n->val.v->name;
    case AST_NODE_FUNCTION:
      return n->val.f->name;
  }
  return NULL;
}

ast_node_t *ast_node_find(ast_node_t *n, char *name) {
  for (; n && (!ast_node_name(n) || strcmp(ast_node_name(n), name));
       n = n->next)
    ;
  return n;
}

int length(ast_node_t *list) {
  int n;
  for (n = 0; list; list = list->next) n++;
  return n;
}

void unchain_last(ast_node_t **list) {
  if (!list || !(*list)) return;
  if (!(*list)->next) {(*list)=NULL;return;}
  ast_node_t *n;
  for(n=*list;n->next->next;n=n->next);
  n->next=NULL;
}


CONSTRUCTOR(ast_t) {
  ALLOC_VAR(r, ast_t);
  r->types = NULL;
  r->functions = NULL;
  r->root_scope = scope_t_new();
  r->current_scope = r->root_scope;
  r->error_occured = 0;
  return r;
}

DESTRUCTOR(ast_t) {
  if (r == NULL) return;
  ast_node_t_delete(r->types);
  ast_node_t_delete(r->functions);
  scope_t_delete(r->root_scope);
  free(r);
}

int ident_role(ast_t *ast, char *ident, ast_node_t **result) {
  int res = IDENT_FREE;
  ast_node_t *nd;

  nd = ast_node_find(ast->functions, ident);
  if (nd) {
    res |= IDENT_FUNCTION;
    if (result) (*result) = nd;
  }

  nd = ast_node_find(ast->root_scope->items, ident);
  if (nd) {
    res |= IDENT_GLOBAL_VAR;
    if (result) (*result) = nd;
  }

  if (ast->current_scope != ast->root_scope)
    for (scope_t *sc = ast->current_scope->parent; sc != ast->root_scope;
         sc = sc->parent) {
      nd = ast_node_find(sc->params, ident);
      if (nd) {
        res |= IDENT_PARENT_LOCAL_VAR;
        if (result) (*result) = nd;
      } else {
        nd = ast_node_find(sc->items, ident);
        if (nd) {
          res |= IDENT_PARENT_LOCAL_VAR;
          if (result) (*result) = nd;
        }
      }
    }

  nd = ast_node_find(ast->current_scope->params, ident);
  if (nd) {
    res |= IDENT_LOCAL_VAR;
    if (result) (*result) = nd;
  } else {
    nd = ast_node_find(ast->current_scope->items, ident);
    if (nd) {
      res |= IDENT_LOCAL_VAR;
      if (result) (*result) = nd;
    }
  }

  return res;
}

