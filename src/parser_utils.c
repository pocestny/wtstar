#ifdef __PARSER_UTILS__

void ignore(void *i) {}  // don't fret about unused values

// add basic  types
#define ADD_STATIC_TYPEDEF(typename, nbytes)                         \
  __type__##typename =                                               \
      ast_node_t_new(NULL, AST_NODE_STATIC_TYPE, strdup(#typename)); \
  __type__##typename->val.t->size = nbytes;                          \
  append(ast_node_t, &ast->types, __type__##typename);

ast_node_t *__type__int, *__type__float, *__type__void, *__type__char;

void add_basic_types(ast_t *ast) {
  ADD_STATIC_TYPEDEF(int, 4)
  ADD_STATIC_TYPEDEF(float, 4)
  ADD_STATIC_TYPEDEF(void, 0)
  ADD_STATIC_TYPEDEF(char, 1)
}

int make_typedef(ast_t *ast, YYLTYPE *rloc, char *ident, YYLTYPE *iloc,
                 static_type_member_t *members) {
  if (!members || !ident || ast->error_occured) {
    static_type_member_t_delete(members);
    free(ident);
    return 1;
  }

  int role = ident_role(ast, ident, NULL);

  if (role != IDENT_FREE) {
    yyerror(rloc, ast, "type name (%s) must be unique", ident);
    static_type_member_t_delete(members);
    free(ident);
    return 0;
  }

  ast_node_t *nt = ast_node_t_new(rloc, AST_NODE_STATIC_TYPE, ident);
  nt->val.t->members = members;
  list_for(m, static_type_member_t, members) {
    m->offset = nt->val.t->size;
    m->parent = nt->val.t;
    nt->val.t->size += m->type->size;
  }
  list_for_end;

  append(ast_node_t, &ast->types, nt);
  free(ident);
  return 1;
}

void add_variable_flag(int flag, ast_node_t *list) {
  list_for(v, ast_node_t, list) v->val.v->io_flag = flag;
  list_for_end;
}

// append list of variables to current scope
int append_variables(ast_t *ast, ast_node_t *list) {
  list_for(v, ast_node_t, list) {
    v->next = NULL;
    if (ident_role(ast, v->val.v->name, NULL) & IDENT_LOCAL_VAR) {
      yyerror(&v->loc, ast, "redefinition of local variable %s",
              v->val.v->name);
      ast_node_t_delete(list);
      ast_node_t_delete(v);
      return 0;
    }
    append(ast_node_t, &ast->current_scope->items, v);
    v->val.v->scope = ast->current_scope;
  }
  list_for_end;
  return 1;
}

ast_node_t *expression_int_val(int val) {
  ast_node_t *zero = ast_node_t_new(NULL, AST_NODE_EXPRESSION, EXPR_LITERAL);
  zero->val.e->type->type = __type__int->val.t;
  zero->val.e->val.l = malloc(sizeof(int));
  *(int *)(zero->val.e->val.l) = val;
  return zero;
}

// update var so that it represents array with dimensions in exprlist
int init_array(ast_t *ast, ast_node_t *var, ast_node_t *exprlist) {
  if (!var) {
    ast_node_t_delete(exprlist);
    return 1;
  }

  if (!exprlist) {
    yyerror(&(var->loc), ast, "array must have at least one dimension");
    return 0;
  }

  for (ast_node_t *e = exprlist; e; e = e->next)
    if (!expr_int(e->val.e)) {
      char *tname = inferred_type_name(e->val.e->type);
      yyerror(&(e->loc), ast, "array dimesion must be integral, has type %s",
              tname);
      free(tname);
      ast_node_t_delete(exprlist);
      return 0;
    }

  variable_t *v = var->val.v;
  v->num_dim = length(exprlist);
  v->ranges = exprlist;

  return 1;
}

int init_input_array(ast_t *ast, ast_node_t *var, int num_dim) {
  if (!var) return 1;

  if (num_dim < 1) {
    yyerror(&(var->loc), ast, "array must have at least one dimension");
    return 0;
  }

  variable_t *v = var->val.v;
  v->num_dim = num_dim;
  v->ranges = NULL;

  return 1;
}

ast_node_t *init_variable(ast_t *ast, YYLTYPE *loc, char *vname) {
  int role = ident_role(ast, vname, NULL);
  if (role & IDENT_LOCAL_VAR) {
    yyerror(loc, ast, "redefinition of variable %s", vname);
    free(vname);
    return NULL;
  }
  if (role & IDENT_FUNCTION) {
    yyerror(loc, ast, "redefinition of %s as different kind of symbol", vname);
    free(vname);
    return NULL;
  }
  ast_node_t *v = ast_node_t_new(loc, AST_NODE_VARIABLE, vname);
  free(vname);
  return v;
}

#define __define_function_abort__ \
  free(name);                     \
  ast_node_t_delete(params);      \
  return NULL;

ast_node_t *define_function(ast_t *ast, YYLTYPE *loc, static_type_t *type,
                            char *name, YYLTYPE *nameloc, ast_node_t *params) {
  ast_node_t *fn;
  int role = ident_role(ast, name, &fn);
  if (role == IDENT_FUNCTION) {
    // check parameters
    function_t *f = fn->val.f;
    if (f->root_scope) {
      yyerror(nameloc, ast, "redefinition of function %s", name);
      __define_function_abort__
    }
    if (f->out_type != type) {
      yyerror(loc, ast, "type mismatch in function definition");
      __define_function_abort__
    }
    if (length(f->params) != length(params)) {
      yyerror(
          loc, ast,
          "definition and declaration of %s disagree on number of parameters",
          name);
      __define_function_abort__
    }
    for (ast_node_t *x = params, *y = f->params; x; x = x->next, y = y->next) {
      if (strcmp(x->val.v->name, y->val.v->name)) {
        yyerror(&y->loc, ast, "parameter name mismatch (%s,%s)", x->val.v->name,
                y->val.v->name);
        __define_function_abort__
      }
      if (x->val.v->base_type != y->val.v->base_type) {
        yyerror(&y->loc, ast, "parameter type mismatch");
        __define_function_abort__
      }
      if (x->val.v->num_dim != y->val.v->num_dim) {
        yyerror(&y->loc, ast, "parameter number of dimensions mismatch");
        __define_function_abort__
      }
    }
    ast_node_t_delete(params);

  } else if (role != IDENT_FREE) {
    yyerror(nameloc, ast, "redefinition of %s as different kind of symbol",
            name);
    __define_function_abort__
  } else {
    fn = ast_node_t_new(loc, AST_NODE_FUNCTION, name);
    fn->val.f->params = params;
    fn->val.f->out_type = type;
    append(ast_node_t, &ast->functions, fn);
  }

  return fn;
}

ast_node_t *create_specifier_expr(YYLTYPE *loc, ast_t *ast, ast_node_t *expr,
                                  char *ident, YYLTYPE *iloc) {
  if (expr->val.e->type->compound) {
    yyerror(loc, ast, "specifier of uncasted type");
    free(ident);
    ast_node_t_delete(expr);
    return 0;
  }

  static_type_member_t *t =
      static_type_member_find(expr->val.e->type->type->members, ident);

  if (!t) {
    yyerror(loc, ast, "type %d dose not have a member %s",
            expr->val.e->type->type->name, ident);
    free(ident);
    ast_node_t_delete(expr);
    return 0;
  }

  ast_node_t *res = ast_node_t_new(loc, AST_NODE_EXPRESSION, EXPR_SPECIFIER);
  res->val.e->type->type = t->type;
  res->val.e->val.s->memb = t;
  res->val.e->val.s->ex = expr;
  free(ident);
  return res;
}

ast_node_t *expression_variable(ast_t *ast, YYLTYPE *loc, char *name) {
  ast_node_t *vn;
  int role = ident_role(ast, name, &vn);
  if (!(role & IDENT_VAR)) {
    yyerror(loc, ast, "%s is not a variable", name);
    free(name);
    return NULL;
  }
  ast_node_t *res = ast_node_t_new(loc, AST_NODE_EXPRESSION, EXPR_VAR_NAME);
  expression_t *e = res->val.e;
  e->type->type = vn->val.v->base_type;
  e->val.v->var = vn->val.v;
  free(name);
  return res;
}

int add_expression_array_parameters(ast_node_t *ve, ast_node_t *p) {
  ve->val.e->variant =  EXPR_ARRAY_ELEMENT;
  // TODO: check type, number of dimensions, etc.
  ve->val.e->val.v->params = p;
  return 1;
}

ast_node_t *expression_call(ast_t *ast, YYLTYPE *loc, char *name,
                            ast_node_t *params) {
  ast_node_t *fn;
  int role = ident_role(ast, name, &fn);
  if (!(role & IDENT_FUNCTION)) {
    yyerror(loc, ast, "%s is not a function", name);
    free(name);
    ast_node_t_delete(params);
    return NULL;
  }
  ast_node_t *res = ast_node_t_new(loc, AST_NODE_EXPRESSION, EXPR_CALL);
  expression_t *e = res->val.e;
  e->type->type = fn->val.f->out_type;
  e->val.f->fn = fn->val.f;
  // TODO: check type, number of dimensions, etc
  e->val.f->params = params;
  free(name);
  return res;
}

// AST_NODE_EXPRESSION initialized with variant and parameters
// check types, and set the resulting type
// on error, return 0 and deallocate node
int fix_expression_type(ast_node_t *node) {
  node->val.e->type->type = __type__int->val.t;
  return 1;
}
#endif
