#ifdef __PARSER_UTILS__

// add basic  types
#define ADD_STATIC_TYPEDEF(type, nbytes)                         \
  ast_node_t *__##type =                                         \
      ast_node_t_new(NULL, AST_NODE_STATIC_TYPE, strdup(#type)); \
  __##type->val.t->size = nbytes;                                \
  append(ast_node_t, &ast->types, __##type);

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

  int role = ident_role(ast, ast->root_scope, ident);

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
#endif
