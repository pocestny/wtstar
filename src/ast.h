#ifndef ___AST_H___
#define ___AST_H___

#include <inttypes.h>
#include <stdarg.h>

#include "utils.h"
#include "writer.h"

struct _ast_node_t;
#define AST_NODE_STATIC_TYPE 1
#define AST_NODE_STATIC_TYPE_MEMBER 2
#define AST_NODE_VARIABLE 4
#define AST_NODE_ARRAY 8
#define AST_NODE_SCOPE 16
#define AST_NODE_FUNCTION 32
#define AST_NODE_EXPRESSION 64


/* static types */
struct _static_type_member_t;

typedef struct {
  uint32_t size;
  char *name;                             // owned
  struct _static_type_member_t *members;  // owned
} static_type_t;

CONSTRUCTOR(static_type_t, char *name);
DESTRUCTOR(static_type_t);

typedef struct _static_type_member_t {
  char *name;                    // owned
  static_type_t *type, *parent;  // external
  uint32_t offset;
  struct _static_type_member_t *next;
} static_type_member_t;

CONSTRUCTOR(static_type_member_t, char *name, static_type_t *type);
DESTRUCTOR(static_type_member_t);
static_type_member_t * static_type_member_find(static_type_member_t *list,char *name);

/* variables */

#define IO_FLAG_NONE 0
#define IO_FLAG_IN 1
#define IO_FLAG_OUT 2

struct _scope_t;

typedef struct {
  uint8_t io_flag;
  char *name;                // owned
  static_type_t *base_type;  // external
  struct _scope_t *scope;    // external
  uint32_t addr;             // address (set during code generation)
} variable_t;

/* arrays */

typedef struct _array_t {
  uint8_t io_flag;
  char *name;                // owned
  static_type_t *base_type;  // external
  struct _scope_t *scope;    // external
  uint8_t num_dim;           // >0
                             // number of active dimensions (subsript length)
  int *active_dims;          // indices of active dimensions (length=num_dim)
  struct _array_t *root;     // either self or the root array of alias chain
  struct _array_t *orig;     // external - original array for arrays
  uint32_t addr;             // address  (set during code generation)
} array_t;

/* scopes */

typedef struct _scope_t {
  struct _scope_t *parent;    // NULL for root_scope (global or function)
  struct _ast_node_t *items;  // owned
} scope_t;

CONSTRUCTOR(scope_t);
DESTRUCTOR(scope_t);

/* functions */

typedef struct {
  char *name;  // owned
  static_type_t *out_type;
  struct _ast_node_t *params;  // list of AST_NODE_VARIABLE or AST_NODE_ARRAY
  scope_t *root_scope;
  uint32_t addr;  // absolute address in code (set during code generation)
} function_t;

/* expressions */
struct _inferred_type_item_t;

typedef struct _inferred_type_t {
  int compound;
  union {
    static_type_t *type;                 // external
    struct _inferred_type_item_t *list;  // owned
  };
} inferred_type_t;

typedef struct _inferred_type_item_t {
  inferred_type_t *type;
  struct _inferred_type_item_t *next, *prev;
} inferred_type_item_t;

#define EXPR_EMPTY 18
#define EXPR_LITERAL 19
#define EXPR_INITIALIZER 20
#define EXPR_FUNCTION 21
#define EXPR_ARRAY_ELEMENT 22
#define EXPR_VAR_NAME 23
#define EXPR_IMPLICIT_ALIAS 24
#define EXPR_SIZEOF 25

struct _expression_t;

typedef struct {
  function_t *fn;
  struct _ast_node_t *params;
} expr_function_t;

typedef struct {
  variable_t *var;
  struct _ast_node_t *params;  // list of expressions
} expr_variable_t;

typedef struct {
  int oper;
  struct _expression_t *first, *second;
} expr_oper_t;

typedef struct {
  static_type_t *type;
  struct _expression_t *ex;
} expr_cast_t;

typedef struct {
  static_type_member_t *type;
  struct _expression_t *ex;
} expr_specif_t;

typedef struct _expression_t {
  inferred_type_t *type;
  int variant;
  union {
    void *l;                // literal: contains value specified by type
    struct _ast_node_t *i;  // initializer: contains flattened list of members
    expr_function_t *f;     // function call
    expr_variable_t *a;     // array element
    expr_variable_t *vn;    // variable name (params are NULL)
    expr_variable_t *ia;    // implicit alias (params are expr:expr, expr:empty)
    expr_variable_t *sz;    // sizeof: one parameter = dimension
    expr_oper_t *post;      // postfix operator
    expr_oper_t *pref;      // prefix operator
    expr_oper_t *bin;       // binary operator
    expr_cast_t *c;         // cast
    expr_specif_t *s;       // specifier
  };

} expression_t;

/* generic AST node */

typedef struct _ast_node_t {
  int node_type;

  YYLTYPE loc;

  union {
    static_type_t *t;
    static_type_member_t *tm;
    variable_t *v;
    array_t *a;
    scope_t *sc;
    function_t *f;
    expression_t *e;
  } val;

  struct _ast_node_t *next;
} ast_node_t;

CONSTRUCTOR(ast_node_t, YYLTYPE *iloc, int node_type, ...);
DESTRUCTOR(ast_node_t);
char * ast_node_name(ast_node_t*n);
ast_node_t *ast_node_find(ast_node_t *n, char *name);

typedef struct {
  ast_node_t *types;      // list of AST_NODE_STATIC_TYPE
  ast_node_t *functions;  // list of AST_NODE_FUNCTION
  scope_t *root_scope;
  int error_occured;
} ast_t;

CONSTRUCTOR(ast_t);
DESTRUCTOR(ast_t);

#define IDENT_FREE 0x0U
#define IDENT_FUNCTION 0x1U
#define IDENT_GLOBAL_VAR 0x2U
// local var is relative to current scope
#define IDENT_LOCAL_VAR 0x4U
#define IDENT_PARENT_LOCAL_VAR 0x8U

#define IDENT_VAR 0xEU

// returns combination of values
int ident_role(ast_t *ast, scope_t *scope, char *ident);

void emit_code(ast_t *ast, writer_t *out, writer_t *log);

#endif
