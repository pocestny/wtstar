/**
 * @file parser_utils.c
 * @brief utilities included directly into parser.y
 *
 * Some basic error checking is done here, and errors are reported using #errors.h
 * while parsing. More checking is done later in #code_generation.h
 */


void ignore(); //!< don't fret about unused values

//! add basic types as global constants
void add_basic_types(ast_t *ast);

//! add built-in functions into ast_t
void add_builtin_functions(ast_t *ast);

//! parse a typedef
void make_typedef(ast_t *ast, YYLTYPE *rloc, char *ident, YYLTYPE *iloc,
                  static_type_member_t *members) ;

//! add a flag to all variables in a list
void add_variable_flag(int flag, ast_node_t *list) ;

//! append list of variables to current scope
void append_variables(ast_t *ast, ast_node_t *list);

//! create an expression literal of specified integer value
ast_node_t *expression_int_val(ast_t *ast, int val);

/**
 * update var so that it represents array with dimensions in exprlist
 * on error, keep the variable as scalar
 */
void init_array(ast_t *ast, ast_node_t *var, ast_node_t *exprlist);

//! parse input array
void init_input_array(ast_t *ast, ast_node_t *var, int num_dim);

//! parse variable
ast_node_t *init_variable(ast_t *ast, YYLTYPE *loc, char *vname);

//! parse function definitions
ast_node_t *define_function(ast_t *ast, YYLTYPE *loc, static_type_t *type,
                            char *name, YYLTYPE *nameloc, ast_node_t *params);

//! parse specifier expression
ast_node_t *create_specifier_expr(YYLTYPE *loc, ast_t *ast, ast_node_t *expr,
                                  char *ident, YYLTYPE *iloc);

//! pasre variable expression
ast_node_t *expression_variable(ast_t *ast, YYLTYPE *loc, char *name);

//! parse sizeof expression
ast_node_t *expression_sizeof(ast_t *ast, YYLTYPE *loc, char *name,
                              ast_node_t *dim);

//! set the dimensions of an array variable
ast_node_t *array_dimensions(ast_t *ast, YYLTYPE *loc, char *name);

//! set the list of array indices
int add_expression_array_parameters(ast_node_t *ve, ast_node_t *p);

//! parse function call expression
ast_node_t *expression_call(ast_t *ast, YYLTYPE *loc, char *name,
                            ast_node_t *params);

//! parse sort expression
ast_node_t *expression_sort(ast_t *ast, YYLTYPE *loc, char *name,
                            ast_node_t *params);

/**
 * AST_NODE_EXPRESSION initialized with variant and parameters
 * check types, and set the resulting type
 * on error, return 0 and deallocate node
 * node is EXPR_BINARY, EXPR_CAST, EXPR_PREFIX or EXPR_POSTFIX
 */
int fix_expression_type(ast_t *ast, YYLTYPE *loc, ast_node_t *node);

#ifdef __PARSER_UTILS__

void ignore(void *i) {}  

// TODO global variables
#define ADD_STATIC_TYPEDEF(ast, typename, nbytes)                           \
  if(!ast->__type__##typename) {                                            \
    ast->__type__##typename =                                               \
        ast_node_t_new(NULL, AST_NODE_STATIC_TYPE, strdup(#typename));      \
    ast->__type__##typename->val.t->size = nbytes;                          \
  }                                                                         \
  {                                                                         \
    int res;                                                                \
    list_find(ast_node_t, &ast->types, ast->__type__##typename, res);       \
    if(!res) list_append(ast_node_t, &ast->types, ast->__type__##typename); \
  }

// add basic types as global constants
void add_basic_types(ast_t *ast) {
  ADD_STATIC_TYPEDEF(ast, int, 4)
  ADD_STATIC_TYPEDEF(ast, float, 4)
  ADD_STATIC_TYPEDEF(ast, void, 0)
  ADD_STATIC_TYPEDEF(ast, char, 1)
}

//TODO duplicates when recompiling
#define NEW_BUILTIN_FUNCTION(ast, name, outtype)            \
  fn = ast_node_t_new(NULL, AST_NODE_FUNCTION, #name); \
  fn->val.f->out_type = ast->__type__##outtype->val.t;

#define BUILTIN_PARAM(ast, name, typename)                 \
  p = ast_node_t_new(NULL, AST_NODE_VARIABLE, #name); \
  p->val.v->base_type = ast->__type__##typename->val.t;    \
  list_append(ast_node_t, &fn->val.f->params, p);

// add built-in functions into ast_t
void add_builtin_functions(ast_t *ast) {
  ast_node_t *fn, *p;

  NEW_BUILTIN_FUNCTION(ast, sqrtf, float)
  BUILTIN_PARAM(ast, x, float)
  list_append(ast_node_t, &ast->functions, fn);

  NEW_BUILTIN_FUNCTION(ast, sqrt, int)
  BUILTIN_PARAM(ast, x, int)
  list_append(ast_node_t, &ast->functions, fn);

  NEW_BUILTIN_FUNCTION(ast, logf, float)
  BUILTIN_PARAM(ast, x, float)
  list_append(ast_node_t, &ast->functions, fn);

  NEW_BUILTIN_FUNCTION(ast, log, int)
  BUILTIN_PARAM(ast, x, int)
  list_append(ast_node_t, &ast->functions, fn);
}

// parse a typedef
void make_typedef(ast_t *ast, YYLTYPE *rloc, char *ident, YYLTYPE *iloc,
                  static_type_member_t *members) {
  if (!members || !ident) {
    static_type_member_t_delete(members);
    free(ident);
    return;
  }

  int role = ident_role(ast, ident, NULL);

  if (role != IDENT_FREE) {
    yyerror(rloc, ast, NULL,
            "name of the newly defined type '%s' already belongs to a %s",
            ident, role_name(role));
    static_type_member_t_delete(members);
    free(ident);
    return;
  }

  ast_node_t *nt = ast_node_t_new(rloc, AST_NODE_STATIC_TYPE, ident);
  nt->val.t->members = members;
  int err = 0;
  list_for(m, static_type_member_t, members) {
    m->offset = nt->val.t->size;
    m->parent = nt->val.t;
    nt->val.t->size += m->type->size;
    if (!strcmp(m->name, ident)) err = 1;
  }
  list_for_end;

  if (err) {
    yyerror(rloc, ast, NULL,
            "name of a member is the same as the newly defined type '%s'",
            ident);
    ast_node_t_delete(nt);
    free(ident);
    return;
  }

  list_append(ast_node_t, &ast->types, nt);
  free(ident);
}

// add a flag to all variables in a list
void add_variable_flag(int flag, ast_node_t *list) {
  list_for(v, ast_node_t, list) v->val.v->io_flag = flag;
  list_for_end;
}

// append list of variables to current scope
void append_variables(ast_t *ast, ast_node_t *list) {
  list_for(v, ast_node_t, list) {
    v->next = NULL;
    if (ident_role(ast, v->val.v->name, NULL) & IDENT_LOCAL_VAR) {
      yyerror(&v->loc, ast, NULL, "redefinition of local variable %s",
              v->val.v->name);
      ast_node_t_delete(v);
    } else {
      list_append(ast_node_t, &ast->current_scope->items, v);
      v->val.v->scope = ast->current_scope;
    }
  }
  list_for_end;
}

// create an expression literal of specified integer value
ast_node_t *expression_int_val(ast_t *ast, int val) {
  ast_node_t *zero = ast_node_t_new(NULL, AST_NODE_EXPRESSION, EXPR_LITERAL);
  zero->val.e->type->type = ast->__type__int->val.t;
  zero->val.e->val.l = malloc(sizeof(int));
  *(int *)(zero->val.e->val.l) = val;
  return zero;
}

// update var so that it represents array with dimensions in exprlist
// on error, keep the variable as scalar
void init_array(ast_t *ast, ast_node_t *var, ast_node_t *exprlist) {
  if (!var) {
    ast_node_t_delete(exprlist);
    return;
  }

  if (!exprlist) {
    yyerror(&(var->loc), ast, NULL, "array '%s' must have at least one dimension",
            var->val.v->name);
    return;
  }

  int current_dim = 1;
  for (ast_node_t *e = exprlist; e; e = e->next, current_dim++)
    if (!expr_int(e->val.e)) {
      char *tname = inferred_type_name(e->val.e->type);
      yyerror(&(e->loc), ast, NULL,
              "%d-th array dimesion must be integral expression, has type %s",
              current_dim, tname);
      free(tname);
      ast_node_t_delete(exprlist);
      return;
    }

  variable_t *v = var->val.v;
  v->num_dim = length(exprlist);
  v->ranges = exprlist;
}

// parse input array
void init_input_array(ast_t *ast, ast_node_t *var, int num_dim) {
  if (!var) return;

  if (num_dim < 1) {
    yyerror(&(var->loc), ast, NULL,
            "input array '%s' must have at least one dimension",
            var->val.v->name);
    return;
  }

  variable_t *v = var->val.v;
  v->num_dim = num_dim;
  v->ranges = NULL;
}

// parse variable
ast_node_t *init_variable(ast_t *ast, YYLTYPE *loc, char *vname) {
  int role = ident_role(ast, vname, NULL);
  if (role & IDENT_LOCAL_VAR) {
    yyerror(loc, ast, NULL, "redefinition of variable %s", vname);
    free(vname);
    return NULL;
  }
  if (role & IDENT_FUNCTION) {
    yyerror(loc, ast, NULL, "redefinition of %s as different kind of symbol", vname);
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

// parse function definitions
ast_node_t *define_function(ast_t *ast, YYLTYPE *loc, static_type_t *type,
                            char *name, YYLTYPE *nameloc, ast_node_t *params) {
  ast_node_t *fn = NULL;
  int role = ident_role(ast, name, &fn);
  if (role == IDENT_FUNCTION) {
    // check parameters
    function_t *f = fn->val.f;
    if (f->root_scope) {
      yyerror(nameloc, ast, NULL, "redefinition of function %s", name);
      __define_function_abort__
    }
    if (!static_type_equal(f->out_type, type)) {
      yyerror(loc, ast, NULL, "type mismatch in function definition");
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
        yyerror(&x->loc, ast, NULL,
                "parameter name mismatch (%s was previously defined as %s)",
                x->val.v->name, y->val.v->name);
        __define_function_abort__
      }
      if (!static_type_equal(x->val.v->base_type, y->val.v->base_type)) {
        yyerror(&y->loc, ast, NULL, "parameter type mismatch");
        __define_function_abort__
      }
      if (x->val.v->num_dim != y->val.v->num_dim) {
        yyerror(&y->loc, ast, NULL, "parameter number of dimensions mismatch");
        __define_function_abort__
      }
    }
    ast_node_t_delete(params);

  } else if (role != IDENT_FREE) {
    yyerror(nameloc, ast, NULL, "redefinition of %s as different kind of symbol",
            name);
    __define_function_abort__
  } else {
    fn = ast_node_t_new(loc, AST_NODE_FUNCTION, name);
    fn->val.f->params = params;
    fn->val.f->out_type = type;
    list_append(ast_node_t, &ast->functions, fn);
  }

  return fn;
}

// parse specifier expression
ast_node_t *create_specifier_expr(YYLTYPE *loc, ast_t *ast, ast_node_t *expr,
                                  char *ident, YYLTYPE *iloc) {
  if (expr->val.e->type->compound) {
    yyerror(loc, ast, NULL, "specifier of uncasted type");
    free(ident);
    ast_node_t_delete(expr);
    return 0;
  }

  static_type_member_t *t =
      static_type_member_find(expr->val.e->type->type->members, ident);

  if (!t) {
    yyerror(loc, ast, NULL, "type %s does not have a member %s",
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

// pasre variable expression
ast_node_t *expression_variable(ast_t *ast, YYLTYPE *loc, char *name) {
  ast_node_t *vn;
  int role = ident_role(ast, name, &vn);
  if (!(role & IDENT_VAR)) {
    yyerror(loc, ast, NULL, "%s is not a variable", name);
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

// parse sizeof expression
ast_node_t *expression_sizeof(ast_t *ast, YYLTYPE *loc, char *name,
                              ast_node_t *dim) {
  ast_node_t *vn;
  int role = ident_role(ast, name, &vn);
  if (!(role & IDENT_VAR)) {
    yyerror(loc, ast, NULL, "%s is not a variable", name);
    free(name);
    ast_node_t_delete(dim);
    return NULL;
  }
  variable_t *var = vn->val.v;
  if (var->num_dim == 0) {
    yyerror(loc, ast, NULL, "%s is not an array", name);
    free(name);
    ast_node_t_delete(dim);
    return NULL;
  }

  if (dim && (!expr_int(dim->val.e))) {
    yyerror(loc, ast, NULL, "non integral array dimension");
    free(name);
    ast_node_t_delete(dim);
    return NULL;
  }

  ast_node_t *res = ast_node_t_new(loc, AST_NODE_EXPRESSION, EXPR_SIZEOF);
  res->val.e->type->type = ast->__type__int->val.t;
  res->val.e->val.v->params = (dim) ? dim : expression_int_val(ast, 0);
  res->val.e->val.v->var = var;
  free(name);
  return res;
}

// set the dimensions of an array variable
ast_node_t *array_dimensions(ast_t *ast, YYLTYPE *loc, char *name) {
  ast_node_t *vn;
  int role = ident_role(ast, name, &vn);
  if (!(role & IDENT_VAR)) {
    yyerror(loc, ast, NULL, "%s is not a variable", name);
    free(name);
    return NULL;
  }
  variable_t *var = vn->val.v;
  if (var->num_dim == 0) {
    yyerror(loc, ast, NULL, "%s is not an array", name);
    free(name);
    return NULL;
  }

  ast_node_t *res = expression_int_val(ast, var->num_dim);
  free(name);
  return res;
}

// set the list of array indices
int add_expression_array_parameters(ast_node_t *ve, ast_node_t *p) {
  ve->val.e->variant = EXPR_ARRAY_ELEMENT;
  ve->val.e->val.v->params = p;
  return 1;
}

// parse function call expression
ast_node_t *expression_call(ast_t *ast, YYLTYPE *loc, char *name,
                            ast_node_t *params) {
  ast_node_t *fn;
  int role = ident_role(ast, name, &fn);
  if (!(role & IDENT_FUNCTION)) {
    yyerror(loc, ast, NULL, "%s is not a function", name);
    free(name);
    ast_node_t_delete(params);
    return NULL;
  }
  ast_node_t *res = ast_node_t_new(loc, AST_NODE_EXPRESSION, EXPR_CALL);
  expression_t *e = res->val.e;
  e->type->type = fn->val.f->out_type;
  e->val.f->fn = fn->val.f;
  e->val.f->params = params;
  free(name);
  return res;
}

// parse sort expression
ast_node_t *expression_sort(ast_t *ast, YYLTYPE *loc, char *name,
                            ast_node_t *params) {
  if (!params) {
    free(name);
    return NULL;
  }
  ast_node_t *var;
  int role = ident_role(ast, name, &var);
  if (!(role & IDENT_VAR)) {
    yyerror(loc, ast, NULL, "%s is not a variable", name);
    free(name);
    ast_node_t_delete(params);
    return NULL;
  }
  if (var->val.v->num_dim != 1) {
    yyerror(loc, ast, NULL,
            "only 1-dimensional arrays can be sorted; %s has %d dimensions",
            name, var->val.v->num_dim);
    free(name);
    ast_node_t_delete(params);
    return NULL;
  }
  ast_node_t *p = params;

  while (p->val.e->variant == EXPR_SPECIFIER) p = p->val.e->val.s->ex;
  if (p->val.e->type->compound ||
      !static_type_equal(var->val.v->base_type, p->val.e->type->type)) {
    yyerror(loc, ast, NULL, "type mismatch");
    free(name);
    ast_node_t_delete(params);
    return NULL;
  }

  ast_node_t *res = ast_node_t_new(loc, AST_NODE_EXPRESSION, EXPR_SORT);
  expression_t *e = res->val.e;
  e->type->type = var->val.v->base_type;
  e->val.v->var = var->val.v;
  e->val.v->params = params;
  free(name);
  return res;
}

// AST_NODE_EXPRESSION initialized with variant and parameters
// check types, and set the resulting type
// on error, return 0 and deallocate node
// node is EXPR_BINARY, EXPR_CAST, EXPR_PREFIX or EXPR_POSTFIX
int fix_expression_type(ast_t *ast, YYLTYPE *loc, ast_node_t *node) {
  expression_t *e = node->val.e;

  if (e->variant == EXPR_BINARY) {
    expr_oper_t *binexpr = e->val.o;
    if (binexpr->first == NULL || binexpr->second == NULL) return 0;
    int equal = inferred_type_equal(binexpr->first->val.e->type,
                                    binexpr->second->val.e->type);

    if ((binexpr->oper == TOK_EQ || binexpr->oper == TOK_NEQ) && equal &&
        !binexpr->first->val.e->type->compound) {
      e->type->type = ast->__type__int->val.t;
    } else if (binexpr->oper == '=' && equal) {
      inferred_type_t_delete(e->type);
      e->type = inferred_type_copy(binexpr->first->val.e->type);
    } else {
      // only implicit type conversion is int->float
      int fi = 1, ff = 1, si = 1, sf = 1;

      if (binexpr->first->val.e->type->compound)
        fi = ff = 0;
      else {
        if (!static_type_equal(binexpr->first->val.e->type->type, ast->__type__int->val.t)) fi = 0;
        if (!static_type_equal(binexpr->first->val.e->type->type, ast->__type__float->val.t)) ff = 0;
      }

      if (binexpr->second->val.e->type->compound)
        si = sf = 0;
      else {
        if (!static_type_equal(binexpr->second->val.e->type->type, ast->__type__int->val.t)) si = 0;
        if (!static_type_equal(binexpr->second->val.e->type->type, ast->__type__float->val.t)) sf = 0;
      }

      if (fi && si)
        e->type->type = ast->__type__int->val.t;
      else if ((fi || ff) && (si || sf)) {
        if (assign_oper(binexpr->oper) && fi)
          e->type->type = ast->__type__int->val.t;
        else if (comparison_oper(binexpr->oper) || binexpr->oper == TOK_AND ||
                 binexpr->oper == TOK_OR) {
          e->type->type = ast->__type__int->val.t;
        } else
          e->type->type = ast->__type__float->val.t;
      } else {
        yyerror(loc, ast, NULL, "type check error %d %d %d %d", fi, ff, si, sf);
        return 0;
      }
    }
  } else if (e->variant == EXPR_CAST) {
    if (e->val.c && e->val.c->type) e->type->type = e->val.c->type;
  } else {
    // unary expression
    if (e->val.o->first) {
      inferred_type_t_delete(e->type);
      e->type = inferred_type_copy(e->val.o->first->val.e->type);
    }
  }
  return 1;
}
#endif
