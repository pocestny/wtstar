#include <code.h>
#include <debug.h>
#include <hash.h>

#include <stdio.h>
#include <stdlib.h>

char **files = NULL;  // an array of pointers to allcated names (in driver)
int n_files = 0;
int *code_source = NULL;  // id of the ast_node that generated this code
int code_size=0;
hash_table_t *nodes = NULL;  // hash node->id to allocated node in ast

void clear_globals() {
  if (files) free(files);
  files=NULL;
  n_files=0;
  if (code_source) free(code_source);
  code_source=NULL;
  code_size=0;
  if (nodes) hash_table_t_delete(nodes);
  nodes = NULL;
}

void gather_scope(scope_t *sc);

void gather_node(ast_node_t *nd) {

  printf("id=%d type=%d code_owner=[%d-%d] ",nd->id,nd->node_type,nd->code_from,nd->code_to);
  if (nd->loc.fn==NULL)
    printf("no location info\n");
  else 
    printf("%s:%d.%d\n",nd->loc.fn,nd->loc.fl,nd->loc.fc);


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
  if (nd->loc.fn) {
    for (int i=nd->code_from;i<=nd->code_to;i++)
      if (code_source[i]==-1) code_source[i]=nd->id;
  }
}

void gather_scope(scope_t *sc) {
  for (ast_node_t *it = sc->items; it; it = it->next) 
    gather_node(it);
}

void emit_debug_sections(writer_t *out, ast_t *ast, int _code_size) {
  clear_globals();
  code_size=_code_size;
  printf("\ncode_size=%d\n",code_size);
  code_source = (int*)malloc(code_size*sizeof(int));
  for (int i=0;i<code_size;i++)code_source[i]=-1;
  nodes = hash_table_t_new(10,NULL);

  gather_scope(ast->root_scope);

  for (int i=0;i<code_size;i++)
    printf("%d ",code_source[i]);
  printf("\n");
}
