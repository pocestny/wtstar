/**
 * @file ast.h
 * @brief AST structure for parser
 *
 * */
#ifndef ___AST_H___
#define ___AST_H___

#include <inttypes.h>
#include <stdarg.h>

#include <utils.h>
#include <writer.h>

struct _ast_node_t;
struct _scope_t;
struct _expression_t;
struct _function_t;

//! types of AST nodes (ast_node_t)
typedef enum {
  AST_NODE_STATIC_TYPE = 52,  //!< basic or user defined type
  AST_NODE_VARIABLE = 53,     //!< variable
  AST_NODE_SCOPE = 54,        //!< lexical scope
  AST_NODE_FUNCTION = 55,     //!< function definition
  AST_NODE_EXPRESSION = 56,   //!< expression
  AST_NODE_STATEMENT = 57     //<! statement
} ast_node_type_t;

// ----------------------------------------------------------------------------
// TYPES
struct _static_type_member_t;

/**
 * @brief built-in and user-defined types
 *
 *
 */
typedef struct {
  uint32_t size;                          //!< size (in bytes) in memory
  char *name;                             //!< name (owned)
  struct _static_type_member_t *members;  //!< list of members (owned)
  uint32_t id;                            //!< for generating debug info
} static_type_t;

//! constuctor
CONSTRUCTOR(static_type_t, char *name);
//! destructor
DESTRUCTOR(static_type_t);

//! return the TYPE_* descriptor (see code.h) of a basic type
//! (for non-basic type assert fails)
int static_type_basic(static_type_t *t);

/**
 * @brief list of members of a static_type_t
 *
 */
typedef struct _static_type_member_t {
  char *name;           //!< name of the member (owned)
  static_type_t *type,  //!< the type of the member (external)
      *parent;          //!< the type where the member belongs to  (external)
  uint32_t offset;      //!< offset (in bytes) in the member type
  struct _static_type_member_t *next;  //!< next member in the linked list
} static_type_member_t;

//!< constructor
CONSTRUCTOR(static_type_member_t, char *name, static_type_t *type);
//!< destructor (removes the whole list)
DESTRUCTOR(static_type_member_t);

static_type_member_t *static_type_member_find(static_type_member_t *list,
                                              char *name);

// ----------------------------------------------------------------------------
// VARIABLES AND ARRAYS

//! flag for I/O variables
typedef enum {
  IO_FLAG_NONE = 0,  //!< internal variable
  IO_FLAG_IN = 1,    //!< input variable
  IO_FLAG_OUT = 2    //!< output variable
} io_flag_t;

/**
 * @brief AST node for a variable
 *
 */
typedef struct _variable_t {
  uint8_t io_flag;  //!< I/O variable
  char *name;       //!< name  (owned)
  static_type_t
      *base_type;          //!< type, for array the type of elements (external)
  struct _scope_t *scope;  //!< lexical scope (external)
  uint32_t addr;           //!< address (set during code generation)
  struct _ast_node_t *initializer;  //!< owned AST_NODE_EXPRESSION

  uint32_t num_dim;  //!< number of dimensions (>0 = array)
  /**
   * static variables are initialized to 0, with the exception of
   * function parameters, and index variables of pardo statements
   */
  int need_init;

  /**
   * defined only for arrays: list of AST_NODE_EXPRESSION; that
   * gives for each dimension the number of elements
   */
  struct _ast_node_t *ranges;
} variable_t;

//! constructor
CONSTRUCTOR(variable_t, char *name);
//! destructor
DESTRUCTOR(variable_t);

// ----------------------------------------------------------------------------
// LEXICAL SCOPES
/**
 *
 * @brief lexical scope
 *
 * all scopes are descendants of ast->root_scope
 *
 */
typedef struct _scope_t {
  struct _scope_t *parent;    //!< NULL for root_scope
  struct _ast_node_t *items;  //!< list for nodes in the scope (owned)
  struct _function_t *fn;     //!< function the scope belongs to or NULL
} scope_t;

//! constructor
CONSTRUCTOR(scope_t);
//! destructor
DESTRUCTOR(scope_t);

// ----------------------------------------------------------------------------
// FUNCTIONS
/**
 *
 * @brief function definition
 *
 */
typedef struct _function_t {
  char *name;               //!< name (owned)
  static_type_t *out_type;  //!< return type (external)
  struct _ast_node_t
      *params;          //!< parameters, list of AST_NODE_VARIABLE (owned)
  scope_t *root_scope;  //!< root scope (owned), parent is ast->root_scope

  uint32_t n,  //!< id -  entry in the fnmap table
      addr;    //!< absolute address in code (set during code generation)
} function_t;

//! constructor
CONSTRUCTOR(function_t, char *name);
//! destructor
DESTRUCTOR(function_t);

// ----------------------------------------------------------------------------
// EXPRESSIONS

//! various types of expression
typedef enum {
  EXPR_EMPTY = 18,          //!< dummy empty expression
  EXPR_LITERAL = 19,        //!< literal value
  EXPR_INITIALIZER = 20,    //!< initializer value
  EXPR_CALL = 21,           //!< function call
  EXPR_ARRAY_ELEMENT = 22,  //!< indexed array
  EXPR_VAR_NAME = 23,       //!< identifier - variable name
  EXPR_SIZEOF = 24,         //!< sizeof keyword
  EXPR_POSTFIX = 25,        //!< postfix expression
  EXPR_PREFIX = 26,         //!< prefix expression
  EXPR_BINARY = 27,         //!< binary expression
  EXPR_CAST = 28,           //!< typecast
  EXPR_SPECIFIER = 29,      //!< specifier expression (<name>.<member>)
  EXPR_SORT = 30            //!< sort built-in
} expression_variant_t;

/*
 * various stuff for inferred types
 *
 */
struct _inferred_type_item_t;

//! inferred type of an expression determined while parsing
typedef struct _inferred_type_t {
  //! compound inferred type contains a list of items (e.g. in an initiator)
  //! otherwise, the inferred type is some static type
  int compound;
  union {
    static_type_t *type;  //!< if not compound: static type (external)
    struct _inferred_type_item_t *list;  //!< if compound, owned
  };
} inferred_type_t;

//! constructor
CONSTRUCTOR(inferred_type_t);
//! destructor
DESTRUCTOR(inferred_type_t);

//! item of an inferred type
typedef struct _inferred_type_item_t {
  inferred_type_t *type;               //!< the type (static or inferred)
  struct _inferred_type_item_t *next;  //!< next item
} inferred_type_item_t;

//! constructor
CONSTRUCTOR(inferred_type_item_t, inferred_type_t *tt);
//! destructor
DESTRUCTOR(inferred_type_item_t);

//! allocate a string with textual description of the type
char *inferred_type_name(inferred_type_t *t);

//! return deep copy of inferred type
inferred_type_t *inferred_type_copy(inferred_type_t *t);

//! appends src do dst; data are reused and src deallocated
//! returns new dst
inferred_type_t *inferred_type_append(inferred_type_t *dst,
                                      inferred_type_t *src);
//! compare inferred types
int inferred_type_equal(inferred_type_t *a, inferred_type_t *b);

//! what conversions are needed in order to assign values of given types
typedef enum {
  CONVERT_FROM_INT = 1U,    //!< from int
  CONVERT_FROM_FLOAT = 2U,  //!< from float
  CONVERT_FROM = 3U,        //!< from
  CONVERT_TO_INT = 4U,      //!< to int
  CONVERT_TO_FLOAT = 8U,    //!< to float
  CONVERT_TO_CHAR = 16U,    //!< to char
  CONVERT_TO = 28U          //!< to
} conversion_flag_t;

/**
 * Test if static_type_t st can be assigned to from a value of static_type_t t.
 * if yes, and #casts is not NULL, an array of n_casts is allocated, and
 * populated with conversion flags
 */
int static_type_compatible(static_type_t *st, static_type_t *t, int **casts,
                           int *n_casts);
//! same as #static_type_compatible only for inferred types
int inferred_type_compatible(static_type_t *st, inferred_type_t *it,
                             int **casts, int *n);
/**
 * Returns  the number of elements of a static type. If #layout is not NULL,
 * allocates and populates an array describing the components (using
 * #type_descriptor_t from #code.h
 */
int static_type_layout(static_type_t *t, uint8_t **layout);
//! same as #static_type_layout only for inferred types
int inferred_type_layout(inferred_type_t *t, uint8_t **layout);

//! expression variant for function calls
typedef struct {
  function_t *fn;  //!< function (external)
  struct _ast_node_t
      *params;  //!< parameters (owned); list of AST_NODE_EXPRESSION
} expr_function_t;

//! expression variant for variables and array elements
typedef struct {
  variable_t *var;  //!< variable (external)
  //! owned; NULL for variables, list of AST_NODE_EXPRESSION for array elements
  struct _ast_node_t *params;
} expr_variable_t;

//! expression variant for expressions with operator
typedef struct {
  int oper;                            //!< operator (TOK_* from parser.h)
  struct _ast_node_t *first, *second;  //!< owned; AST_NODE_EXPRESSION
} expr_oper_t;

//! expression variant for typecast
typedef struct {
  static_type_t *type;     //!< type to cast to (external)
  struct _ast_node_t *ex;  //!< owned; AST_NODE_EXPRESSION
} expr_cast_t;

//! expression variant for specifier (variable.member)
typedef struct {
  static_type_member_t *memb;  //!< member (external)
  struct _ast_node_t *ex;      //!< owned; AST_NODE_EXPRESSION
} expr_specif_t;

/**
 * @brief expression definition
 *
 * based on variant, the value contains
 *
 * variant      |   type           | meaning
 * -------------|------------------|----------
 * empty        |                  | just a placeholder
 * literal      | void*            | value according to  type
 * initializer  | ast_node_t*      | flattened list of members
 * function call| expr_function_t* | function_t* and parameters (ast_node_t*)
 * variable name| expr_variable_t* | variable_t* (parameters are NULL)
 * array element| expr_variable_t* | variable_t* and parameters (ast_node_t*)
 * sizeof       | expr_variable_t* | variable_t* and one parameter = dimension
 * all operators| expr_oper_t *    | oper (TOK_*), expression_t* first,* second
 * typecast     | expr_cast_t *    | static_type_t* new type, expression_t*
 * orig. ex. specifier    | expr_specif_t *  | static_type_member_t* member,
 * expression_t ex sort         | expr_variable_t *| var = array to be sorted,
 * params = specifier list
 *
 */
typedef struct _expression_t {
  /** for arrays and variables the inferred type points to a static type
   *  for initializers, it is the list of inferred types */
  inferred_type_t *type;
  int variant;  //!< variant
  union {
    uint8_t *l;
    struct _ast_node_t *i;
    expr_function_t *f;
    expr_variable_t *v;
    expr_oper_t *o;
    expr_cast_t *c;
    expr_specif_t *s;
  } val;  //!< the value of the variant

} expression_t;

//! constructor
CONSTRUCTOR(expression_t, int variant);
//! desctructor
DESTRUCTOR(expression_t);

/** is the type integer? (ie either non composed integral type or compound type
  with single integral component
  */
int expr_int(expression_t *e);

// ----------------------------------------------------------------------------
// STATEMENTS
typedef enum {
  STMT_COND = 69,       //!< condition
  STMT_WHILE = 70,      //!< while loop
  STMT_DO = 71,         //!< do .. while loop
  STMT_FOR = 72,        //!< for loop
  STMT_PARDO = 73,      //!< pardo
  STMT_RETURN = 75,     //!< return
  STMT_BREAKPOINT = 76  //!< breapoint
} statement_variant_t;

/**
 * @brief statements
 *
 * The parameters are used as follows:
 *
 * - STMT_COND:
 *    - par[0] expression
 *    - par[1] a scope with two items: then and else
 *
 * - STMT_WHILE and STMT_DO:
 *   - par[0]  expression
 *   - par[1]  scope with the statement
 *
 * - STMT_FOR:
 *    - par[0]  encapsulating scope
 *              contains 4 nodes: three sections from the "for" definition
 *              and one resulting statement
 *
 * - STMT_PARDO:
 *   - par[0] scope with first item the driving variable
 *              contains the statement
 *   - par[1]  expression
 *
 * - STMT_RETURN:
 *   - par[0] expression
 *
 * - STMT_BREAKPOINT:
 *   - par[0] = expression
 */
typedef struct _statement_t {
  int variant;                 //!< variant
  struct _ast_node_t *par[2];  //!< parameters ( owned)
  int tag;                     //!< used for breakpoints
  function_t *ret_fn;          //!< external, only used for return
} statement_t;

//! constructor
CONSTRUCTOR(statement_t, int variant);
//! destructor
DESTRUCTOR(statement_t);

// ----------------------------------------------------------------------------
/**
 *
 * @brief list of AST nodes
 *
 * The basic building block of the AST,  a linked list of AST nodes.
 *
 */
typedef struct _ast_node_t {
  int node_type;  //!< type of node #ast_node_type_t
  YYLTYPE loc;    //!< location in source (YYLTYPE defined in utils.h)
  int id,         //!< each node gets a unique id in the constructor
      code_from,  //!< where in the binary starts the code for this node
      code_to;    //!< where in the binary ends the code for this node
  union {
    static_type_t *t;
    variable_t *v;
    scope_t *sc;
    function_t *f;
    expression_t *e;
    statement_t *s;
  } val;  //!< variant for different types of nodes

  struct _ast_node_t *next;  //!< next node in the linked list
  int emitted;  //!< used in code generation - whether the code has already been
                //!< emitted
} ast_node_t;

/**
 * @brief allocate an ast_node_t variable
 *
 * The constructor takes a location (or NULL), and the type of node
 * (#ast_node_type_t). Further parameters are specific to node types
 *
 *    node type         | parameter
 * ---------------------|-----------
 * AST_NODE_STATIC_TYPE | name (char*)
 * AST_NODE_VARIABLE    | name (char*)
 * AST_NODE_SCOPE       | parent_scope (#scope_t*)
 * AST_NODE_FUNCTION    | name (char*)
 * AST_NODE_EXPRESSION  | variant (int)
 * AST_NODE_STATEMENT   | variant (int)
 *
 *
 * for AST_NODE_EXPRESSION, there are further parameters:
 *
 * variant              | parameters
 * ---------------------|----------------
 * EXPR_BINARY          | left , oper, right (int oper, ast_node_t* left,right)
 * EXPR_CAST            | type, cast_expr (static_type_t*,ast_node_t*, as above)
 * EXPR_PREFIX          | oper, expr
 * EXPR_POSTFIX         | expr, oper
 *
 */
CONSTRUCTOR(ast_node_t, YYLTYPE *iloc, int node_type, ...);

//! free the whole chain (node and all next)
DESTRUCTOR(ast_node_t);

//! for named nodes (static_type,variable,function) return the name, else NULL
char *ast_node_name(ast_node_t *n);

//! in the list of nodes, find one with given name
ast_node_t *ast_node_find(ast_node_t *n, char *name);

//! length of list of nodes
int length(ast_node_t *list);

//! unchain the last element of the list
void unchain_last(ast_node_t **list);

// ----------------------------------------------------------------------------
/**
 * @brief main AST structure
 */
typedef struct {
  ast_node_t *types;      //!< list of AST_NODE_STATIC_TYPE
  ast_node_t *functions;  //!< list of AST_NODE_FUNCTION
  scope_t *root_scope,    //!< global scope, also parent of all function scopes
      *current_scope;     //!< used while parsing
  static_type_t *current_type;  //!< used while parsing
  int error_occured;            //!< flag if parsing was correct
  int mem_mode;                 //!< last issued token for memory mode
} ast_t;

//! allocates the ast_t structure and returns pointer
CONSTRUCTOR(ast_t);
//! free the structure and its members
DESTRUCTOR(ast_t);

//----------------------------------------------------------------------------
/**
 * @brief identifier roles
 */
typedef enum {
  IDENT_FREE = 0x0U,        //!< unused identifier
  IDENT_FUNCTION = 0x1U,    //!< function
  IDENT_GLOBAL_VAR = 0x2U,  //!< global variable
  IDENT_LOCAL_VAR =
      0x4U,  //!< local variable is relative to current scope;  in root
             //!< scope, global variable is both local and global
  IDENT_PARENT_LOCAL_VAR =
      0x8U,  //!< variable is local in some parent scope other from root_scope
  IDENT_VAR = 0xEU  //!<  local or globar or parent local
} ident_role_t;

//! returns combination of values in current scope, and (if not NULL) stores
//! the node (of the closest variable)
int ident_role(ast_t *ast, char *ident, ast_node_t **result);
//! given ident_role_t return description of the role
const char *role_name(int role);

#endif
