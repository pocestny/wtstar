/** 
 * @file ast.h
 *
 * */
#ifndef ___AST_H___
#define ___AST_H___

#include <inttypes.h>
#include <stdarg.h>

#include <utils.h>
#include <writer.h>



/* ----------------------------------------------------------------------------
 * ast_node_t
 *
 * list of ast nodes
 *
 * contains:
 * ast_node_t *next, YYLTYPE loc, int node_type,
 * variant val (t,tm,v,sc,f,e,s): <node_type>_t *
 */

struct _ast_node_t;
struct _scope_t;
struct _expression_t;
struct _function_t;

#define AST_NODE_STATIC_TYPE 52
#define AST_NODE_VARIABLE 53
#define AST_NODE_SCOPE 54
#define AST_NODE_FUNCTION 55
#define AST_NODE_EXPRESSION 56
#define AST_NODE_STATEMENT 57

/* ----------------------------------------------------------------------------
 * static types
 *
 * contains:
 * size,name,list of members
 *
 * 
 */
struct _static_type_member_t;

typedef struct {
  uint32_t size;
  char *name;                             // owned
  struct _static_type_member_t *members;  // owned
  uint32_t id; // for generating debug info
} static_type_t;

CONSTRUCTOR(static_type_t, char *name);
DESTRUCTOR(static_type_t);

// return the TYPE_* descriptor (see code.h) of a basic type
// (for non-basic type assert fails)
int static_type_basic(static_type_t *t);

/*
 * (list of) members of a static type
 *
 * contains
 * name, link to parent type (where it belongs),
 * link to type, offset within parent type
 *
 */

typedef struct _static_type_member_t {
  char *name;                    // owned
  static_type_t *type, *parent;  // external
  uint32_t offset;
  struct _static_type_member_t *next;
} static_type_member_t;

CONSTRUCTOR(static_type_member_t, char *name, static_type_t *type);
DESTRUCTOR(static_type_member_t);  // removes the list until end

static_type_member_t *static_type_member_find(static_type_member_t *list,
                                              char *name);

/* ----------------------------------------------------------------------------
 * variables and arrays
 *
 */

#define IO_FLAG_NONE 0
#define IO_FLAG_IN 1
#define IO_FLAG_OUT 2

typedef struct _variable_t {
  uint8_t io_flag;
  char *name;                       // owned
  static_type_t *base_type;         // external
  struct _scope_t *scope;           // external
  uint32_t addr;                    // address (set during code generation)
  struct _ast_node_t *initializer;  // owned AST_NODE_EXPRESSION

  uint32_t num_dim;  // >0 = array
  int need_init;    // arrays,parameters and pardo driving variables don't

  // following is defined only for arrays
  struct _ast_node_t *ranges;  // expressions; for each dimension the number of
                               // elements n (0..n-1)
} variable_t;

CONSTRUCTOR(variable_t, char *name);
DESTRUCTOR(variable_t);

/* ----------------------------------------------------------------------------
 * scopes
 *
 * lexical scope
 * all scopes are descendants of ast->root_scope
 *
 */

typedef struct _scope_t {
  struct _scope_t *parent;    // NULL for root_scope
  struct _ast_node_t *items;  // owned
  struct _function_t *fn;     // scope belongs to a function
} scope_t;

CONSTRUCTOR(scope_t);
DESTRUCTOR(scope_t);

/* ----------------------------------------------------------------------------
 * functions
 *
 */

typedef struct _function_t {
  char *name;                  // owned
  static_type_t *out_type;     // external
  struct _ast_node_t *params;  // (owned) list of AST_NODE_VARIABLE
  scope_t *root_scope;         // owned

  uint32_t n,  // entry in the fnmap table
      addr;    // absolute address in code (set during code generation)
} function_t;

CONSTRUCTOR(function_t, char *name);
DESTRUCTOR(function_t);

/* ----------------------------------------------------------------------------
 * expressions
 *
 * contains:
 *
 * inferred type
 * either static_type  ( points to already defined types) or list of
 * inferred types
 *
 * for arrays and variables the inferred type points to a static type
 * for initializers, it is the list of inferred types
 *
 * variant: (l,i,f,v,o,c,s)
 *
 * empty: just a placeholder
 * literal: (void*) value according to  type
 * initializer: (ast_node_t*)  flattened list of members (tree structure is 
 *                in the inferred type)
 * function call: (expr_function_t*) function_t* and parameters (ast_node_t*)
 * variable name: (expr_variable_t*) variable_t* (parameters are NULL)
 * array element: (expr_variable_t*) variable_t* and parameters (ast_node_t*)
 * sizeof: (expr_variable_t*) variable_t* and one parameter = dimension
 * operator (postfix,prefix,binary):
 *                 (expr_oper_t *) oper (TOK_*), expression_t *first,*second
 * typecast: (expr_cast_t *) static_type_t* new type, expression_t* orig. ex.
 * specifier: (expr_specif_t *) static_type_member_t* member, expression_t ex
 * sort: (expr_variable_t *) var = array to be sorted, params = specifier list 
 *
 */
#define EXPR_EMPTY 18
#define EXPR_LITERAL 19
#define EXPR_INITIALIZER 20
#define EXPR_CALL 21
#define EXPR_ARRAY_ELEMENT 22
#define EXPR_VAR_NAME 23
#define EXPR_SIZEOF 24
#define EXPR_POSTFIX 25
#define EXPR_PREFIX 26
#define EXPR_BINARY 27
#define EXPR_CAST 28
#define EXPR_SPECIFIER 29
#define EXPR_SORT 30

/*
 * stuff for inferred types
 *
 */
struct _inferred_type_item_t;

typedef struct _inferred_type_t {
  int compound;
  union {
    static_type_t *type;                 // if not compound: external
    struct _inferred_type_item_t *list;  // if compound: owned
  };
} inferred_type_t;

CONSTRUCTOR(inferred_type_t);
DESTRUCTOR(inferred_type_t);

typedef struct _inferred_type_item_t {
  inferred_type_t *type;
  struct _inferred_type_item_t *next;
} inferred_type_item_t;

CONSTRUCTOR(inferred_type_item_t, inferred_type_t *tt);
DESTRUCTOR(inferred_type_item_t);

// allocate a string with textual description of the type
char *inferred_type_name(inferred_type_t *t);

// return deep copy of inferred type
inferred_type_t *inferred_type_copy(inferred_type_t *t);

// appends src do dst; data are reused and src deallocated
// returns new dst
inferred_type_t *inferred_type_append(inferred_type_t *dst,
                                      inferred_type_t *src);

int inferred_type_equal(inferred_type_t *a, inferred_type_t *b);

// can this inferred type be assigned to the static type?
#define CONVERT_FROM_INT 1U
#define CONVERT_FROM_FLOAT 2U
#define CONVERT_FROM 3U
#define CONVERT_TO_INT 4U
#define CONVERT_TO_FLOAT 8U
#define CONVERT_TO_CHAR 16U
#define CONVERT_TO 28U
// casts = if not null and the result is true, 
//    its allocated to layout size, and populated with conversion flags, and n is set
int static_type_compatible(static_type_t *st, static_type_t *t, int **casts, int *n_casts);
int inferred_type_compatible(static_type_t *st, inferred_type_t *it,int **casts,int *n);

/* ----------------------------------------------------------------------------
 * static/inferred type layout
 *
 * returns the number of elements of a static/inferred type
 *
 * if layout is not NULL, allocate and populate an array describing the
 * components
 *
 */

int static_type_layout(static_type_t *t, uint8_t **layout);
int inferred_type_layout(inferred_type_t *t, uint8_t **layout);


/*
 * data storage for variants
 *
 */
typedef struct {
  function_t *fn;              // external
  struct _ast_node_t *params;  // owned; list of AST_NODE_EXPRESSION
} expr_function_t;

typedef struct {
  variable_t *var;             // external
  struct _ast_node_t *params;  // owned; list of AST_NODE_EXPRESSION
} expr_variable_t;

typedef struct {
  int oper;
  struct _ast_node_t *first, *second;  // owned; AST_NODE_EXPRESSION
} expr_oper_t;

typedef struct {
  static_type_t *type;     // external
  struct _ast_node_t *ex;  // owned; AST_NODE_EXPRESSION
} expr_cast_t;

typedef struct {
  static_type_member_t *memb;  // external
  struct _ast_node_t *ex;      // owned; AST_NODE_EXPRESSION
} expr_specif_t;

/*
 * main expression stuff
 *
 */
typedef struct _expression_t {
  inferred_type_t *type;
  int variant;
  union {
    uint8_t *l;
    struct _ast_node_t *i;
    expr_function_t *f;
    expr_variable_t *v;
    expr_oper_t *o;
    expr_cast_t *c;
    expr_specif_t *s;
  } val;

} expression_t;

CONSTRUCTOR(expression_t, int variant);
DESTRUCTOR(expression_t);

// is the type integer? (ie either non composed integral type or compound type
// with single integral component
int expr_int(expression_t *e);

/* ----------------------------------------------------------------------------
 *
 * statements
 *
 * contains at most three parameters (ast_node_t*)
 */
#define STMT_COND 69
#define STMT_WHILE 70
#define STMT_DO 71
#define STMT_FOR 72
#define STMT_PARDO 73
#define STMT_RETURN 75
#define STMT_BREAKPOINT 76

typedef struct _statement_t {
  int variant;
  struct _ast_node_t *par[2];  // owned
  int tag; // used for breakpoints
  function_t *ret_fn; // external, only used for return
} statement_t;

CONSTRUCTOR(statement_t, int variant);
DESTRUCTOR(statement_t);

/* ----------------------------------------------------------------------------
 *
 * AST node
 *
 * constructor paramters: loc + variant + ...
 *
 * AST_NODE_STATIC_TYPE: ident (char*)
 * AST_NODE_VARIABLE: ident (char*)
 * AST_NODE_SCOPE:      parent_scope
 * AST_NODE_FUNCTION: name (char*)
 * AST_NODE_EXPRESSION: variant
 *    for EXPR_BINARY: left , oper, right (int oper, ast_node_t* left,right)
 *          left,right are AST_NODE_EXPRESSIONs
 *    for EXPR_CAST:   type, cast_expr (static_type_t*,ast_node_t*, as above)
 *    for EXPR_PREFIX:  oper, expr
 *    for EXPR_POSTFIX: expr, oper
 * AST_NODE_STATEMENT:  variant
 *
 */

typedef struct _ast_node_t {
  int node_type;
  YYLTYPE loc;
  int id,code_from,code_to;
  union {
    static_type_t *t;
    variable_t *v;
    scope_t *sc;
    function_t *f;
    expression_t *e;
    statement_t *s;
  } val;

  struct _ast_node_t *next;
  int emitted;
} ast_node_t;

CONSTRUCTOR(ast_node_t, YYLTYPE *iloc, int node_type, ...);
DESTRUCTOR(ast_node_t);

// for named nodes (static_type,variable,function) return the name, else NULL
char *ast_node_name(ast_node_t *n);

// in the list of nodes, find one with given name
ast_node_t *ast_node_find(ast_node_t *n, char *name);

// length of list of nodes
int length(ast_node_t *list);

// unchain the last element of the list
void unchain_last(ast_node_t **list);

// ----------------------------------------------------------------------------
/**
 * @brief main AST structure
 */
typedef struct {
  ast_node_t *types;      // list of AST_NODE_STATIC_TYPE
  ast_node_t *functions;  // list of AST_NODE_FUNCTION
  scope_t *root_scope, *current_scope;
  static_type_t *current_type;
  int error_occured;
  int mem_mode; // last issued token for memory mode
} ast_t;

//! allocates the ast_t structure and returns pointer
CONSTRUCTOR(ast_t);
//! free the structure and its members
DESTRUCTOR(ast_t);

/* ----------------------------------------------------------------------------
 *
 * stuff
 *
 */

#define IDENT_FREE 0x0U
#define IDENT_FUNCTION 0x1U
#define IDENT_GLOBAL_VAR 0x2U
// local var is relative to current scope
// in root scope, global variable is both local and global
#define IDENT_LOCAL_VAR 0x4U
// variable is local in some parent scope other from root_scope
#define IDENT_PARENT_LOCAL_VAR 0x8U

#define IDENT_VAR 0xEU  // local or globar or parent local

// returns combination of values in current scope, and (if not NULL) stores
// the node (of the closest variable)
int ident_role(ast_t *ast, char *ident, ast_node_t **result);
const char* role_name(int role);

#endif
