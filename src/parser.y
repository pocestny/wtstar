%code top{
 // helper functions that are local for parser (basically those that call yyerror(...) )
 #include "parser_utils.h"
}

%code requires {
  #include <math.h>
  #include <string.h>
  #include <stdarg.h>
  #include <stdlib.h>
  #include "ast.h"

  #define YYLTYPE YYLTYPE
  typedef struct YYLTYPE
  {
    int fl,fc,ll,lc;
    char *fname;
  } YYLTYPE;

}

%debug
%defines
%locations
%define parse.error verbose
%param {ast_node_t *ast}
%param {context_t *env}
%define api.pure full

%code provides {
typedef struct YYLTYPE YYLTYPE;
struct YYLTYPE
{
  int first_line;
  int first_column;
  int last_line;
  int last_column;
};

   #define YY_DECL \
       int yylex(YYSTYPE * yylval_param, YYLTYPE * yylloc_param, ast_t* ast, context_t *env)
   YY_DECL;
  void yyerror (YYLTYPE *yylloc, ast_node_t *, context_t *, char const *, ...);

}

%initial-action {
}

%define api.token.prefix {TOK_} 
%union
{
  int32_t       int_val;
  float         float_val;
  char          char_val;
  char*         string_val;
  ast_node_t*   ast_node_t_val;
}

%token <int_val>    INT_LITERAL    
%token <float_val>  FLOAT_LITERAL  
%token <char_val>   CHAR_LITERAL   
%token <string_val> STRING_LITERAL IDENT TYPENAME 

%token TYPE INPUT OUTPUT ALIAS IF ELSE FOR WHILE PARDO DO BREAK RETURN CONTINUE SIZE DIM

%token < > DBLBRACKET "[["
%token < > DBRBRACKET "]]"

%token < > EQ "=="
%token < > NEQ "!="
%token < > LEQ "<="
%token < > GEQ ">="
%token < > AND "&&"
%token < > OR "||"

%token < > SHL "<<"
%token < > SHR ">>"
%token < > FIRST_BIT "|~"
%token < > LAST_BIT "~|"

%token < > INC "++"
%token < > DEC "--"

%token < > PLUS_ASSIGN "+="
%token < > MINUS_ASSIGN "-="
%token < > TIMES_ASSIGN "*="
%token < > DIV_ASSIGN "/="
%token < > MOD_ASSIGN "%="
%token < > POW_ASSIGN "^="

%token < > DONT_CARE "_"


%destructor { if ($$) free($$); $$=NULL;} <string_val>

%type <int_val> placeholder_list array_dim_spec add_operator assign_operator eq_operator rel_operator mult_operator unary_operator
%type <ast_node_t_val> expr expr_assign expr_or expr_and expr_eq expr_rel expr_add expr_mult expr_pow expr_cast expr_unary expr_postfix initializer_list initializer_item maybe_expr expr_primary

%%
  
  /* ************************* PROGRAM ***************************** 
      the golbal structure of a program (includes are handled in the lexer)
  */

program: preamble  stmt_scope;

preamble: %empty | preamble preamble_item;

preamble_item: 
  type_def | 
  variable_declaration | 
  input_declaration | 
  output_declaration | 
  alias_declaration | 
  function_declaration ;

  
  /* ******************************* TYPES *********************************** 

      Handles the definition of new types. 
      
      Types are recursive structs of basic types (int,float,char), i.e. array cannot
      be part of a type.
      
      type <new_type_name> {
        <type_1> <name_1_1>, ... ,<name_1_n1>;
        <type_2> <name_2_1>, ... ,<name_2_n2>;
        ...
      }

   */

      
type_def: 
    type_def__1 '{' type_def_list '}'
    |
    type_def__1 error '}'
    ;

type_def__1: 
    TYPE IDENT;


type_def_list: type_def_item |  type_def_list type_def_item ;

type_def_item: 
    type_specifier typedef_id_list ';'
    |
    type_specifier error ';'
    ;

type_specifier: TYPENAME;

typedef_id_list: 
    typedef_id 
    | 
    typedef_id_list ',' typedef_id 
    | 
    error ',' typedef_id
    ;

typedef_id: IDENT;
  
  /* ******************************* VARIABLES  ***********************************
 
      variables can be of static type (basic or defined), arrays, or alises

      <type> <new_name>;
      <type> <new_name> = <value> ;
      
      <type> <new_name> [<size_1>,<size_2>,...,<size_d>];
      
      alias <new_name> = <existing_array> [[ <from>:<to> , <fixed>, <from>:<to> ]];
     
      variables other from alias at global scope can be input / output 

      input arrays don't specify sizes, only dimensions:
      input <type> <name> [ _ , ... , _ ];

  */

input_declaration: 
    INPUT  input_variable_declaration 
    |
    INPUT error ';' 
    ;

output_declaration: 
    OUTPUT variable_declaration 
    |
    OUTPUT error ';'
    ;

alias_declaration: 
    alias_declaration__1 ranges_list "]]" ';'
    | 
    alias_declaration__1 error ';' 
    ;

alias_declaration__1: ALIAS IDENT '=' IDENT "[[" ;

ranges_list: 
    range 
    |
    ranges_list ',' range
    ;

range: 
    expr_assign ':' expr_assign 
    | 
    expr_assign
    ;


  /* possibly declare several variables of the same type */
variable_declaration: 
    type_specifier variable_declarator_list ';' 
    |
    type_specifier error ';' 
    ;

variable_declarator_list: 
    variable_init_declarator 
    | 
    variable_declarator_list ',' variable_init_declarator 
    | 
    error ','  variable_init_declarator
    ;


  /* declare variable, and possibly initialize it */
variable_init_declarator: 
    variable_declarator 
    | 
    variable_declarator '=' initializer_item 
    ;

variable_declarator: 
    static_variable_declarator 
    | 
    static_variable_declarator '[' expr_list ']'
    ;


  /*  ***************************************
      input variables are handled differently

  */
input_variable_declaration: 
    type_specifier input_variable_declarator_list ';' 
    |
    type_specifier error ';' 
    ;

input_variable_declarator_list: 
    input_variable_declarator 
    | 
    input_variable_declarator_list ',' input_variable_declarator 
    | 
    error ',' input_variable_declarator
    ;

input_variable_declarator: 
    static_variable_declarator 
    | 
    static_variable_declarator '[' placeholder_list ']' 
    ;


placeholder_list: 
    '_' {$$=1;}
    |
    placeholder_list ',' "_" {$$=$1+1;}
    ;


  /*
      end of input variables specific part
      ***************************************
  */

static_variable_declarator: IDENT;



  /* ******************************* FUNCTIONS  *********************************** 
  
      functions are declared in C-like style

      <output_type> <function_name> ( <param_1> , .... , <param_n> ) { <body> }

      it is possible to have separate declaration    
  
      <output_type> <function_name> ( <param_1> , .... , <param_n> ) ;
  
  */


function_declaration: 
    function_declaration__1 parameter_declarator_list ')' maybe_body 
    |
    error ')' 
    ;


function_declaration__1: 
    type_specifier IDENT '(' 
    ;

maybe_body: 
    ';'
    | 
    stmt_scope 
    ;

parameter_declarator_list: 
    %empty 
    |  
    nonempty_parameter_declarator_list
    ;

nonempty_parameter_declarator_list: 
    parameter_declarator 
    | 
    nonempty_parameter_declarator_list ',' parameter_declarator
    ;

parameter_declarator: TYPENAME IDENT array_dim_spec;

 
array_dim_spec: %empty {$$=0;} | '{' placeholder_list '}' {$$=$2;};                                  
  
  
  /* ************************* EXPRESSIONS  ***************************** */

expr: 
    expr_assign {$$=$1;} 
    |  
    expr ',' expr_assign 
    ;

expr_assign:
    expr_or {$$=$1;} 
    | 
    expr_unary  assign_operator expr_assign 
    ;

expr_or: 
    expr_and {$$=$1;} 
    | 
    expr_or OR expr_and
    ;

expr_and: 
    expr_eq {$$=$1;} 
    | 
    expr_and AND expr_eq
    ;


expr_eq:  
    expr_rel {$$=$1;} 
    | 
    expr_eq  eq_operator expr_rel
    ;


expr_rel:  
    expr_add {$$=$1;} 
    | 
    expr_rel  rel_operator  expr_add
    ;


expr_add: 
    expr_mult {$$=$1;} 
    | 
    expr_add    add_operator    expr_mult
    ;

expr_mult: 
    expr_pow {$$=$1;} 
    | 
    expr_mult   mult_operator   expr_pow
    ;


expr_pow:  
    expr_cast {$$=$1;} 
    | 
    expr_cast '^' expr_pow
    ;


expr_cast: 
    expr_unary {$$=$1;} 
    | 
    '(' TYPENAME ')'  expr_cast
    | '(' TYPENAME ')' '{' initializer_list '}'
    ;

expr_unary:   
    expr_postfix {$$=$1;}  
    | 
    unary_operator expr_postfix
    ;


expr_postfix: 
    expr_primary  {$$=$1;}
    | 
    expr_postfix '.' IDENT
    |
    expr_postfix INC 
    | 
    expr_postfix DEC
    ;

assign_operator: 
      '=' {$$='=';} 
    | PLUS_ASSIGN {$$=TOK_PLUS_ASSIGN;} 
    | MINUS_ASSIGN {$$=TOK_MINUS_ASSIGN;}
    | TIMES_ASSIGN {$$=TOK_TIMES_ASSIGN;} 
    | DIV_ASSIGN {$$=TOK_DIV_ASSIGN;} 
    | MOD_ASSIGN {$$=TOK_MOD_ASSIGN;}
    ;

eq_operator: 
      EQ  {$$=TOK_EQ;} 
    | NEQ {$$=TOK_NEQ;}
    ;

rel_operator: 
      '<' {$$='<';}
    | '>' {$$='>';}
    | LEQ {$$=TOK_LEQ;}
    | GEQ{$$=TOK_GEQ;}
    ;

add_operator: 
      '+' {$$='+';} 
    | '-' {$$='-';}
    ;

mult_operator: 
      '*' {$$='*';}
    | '/'{$$='/';} 
    | '%'{$$='%';}
    ;

unary_operator: 
      '!' {$$='!';}
    | '-' {$$='-'}
    | INC {$$=TOK_INC;}
    | DEC {$$=TOK_DEC;}
    ;
  

expr_primary: 
    IDENT  
    |
    IDENT SIZE 
    |
    IDENT SIZE "(" expr_assign ")" 
    |
    IDENT "[["  ranges_list "]]"  
    |
    IDENT '[' expr_list ']' 
    | 
    IDENT '(' expr_list ')' 
    | 
    IDENT '(' ')' 
    | 
    '(' expr ')'
    |
    expr_literal 
    ;


expr_literal: 
    INT_LITERAL
    |
    FLOAT_LITERAL 
    | 
    CHAR_LITERAL 
    | 
    STRING_LITERAL 
    ;


  /* bracketed comma separated initializer list - returns expression */
initializer_list: 
    initializer_item {$$=$1;} 
    | 
    initializer_list ',' initializer_item 
    |
    error ',' initializer_item 
    ;

initializer_item: 
    expr_assign {$$=$1;} 
    | 
    '{' initializer_list '}'
    ;


  /* list of expressions */
expr_list: 
    expr_assign 
    | 
    expr_list ',' expr_assign 
    |
    error ',' expr_assign 
    ;


  /* ************************* STATEMENTS  ***************************** */

stmt: stmt_scope | stmt_expr  | stmt_cond | stmt_iter | stmt_jump ';' | error ';';

stmt_scope: 
    stmt_scope__1  scope_item_list '}'
    ;

stmt_scope__1: '{' ;

scope_item_list: %empty | scope_item_list scope_item ;
scope_item: variable_declaration | stmt;

stmt_expr: 
    expr ';'
    ;

stmt_cond: IF '(' expr ')' stmt maybe_else ;
maybe_else: %empty | ELSE stmt ;

stmt_iter: 
    WHILE '(' expr ')'  stmt
    |
    DO stmt WHILE '(' expr ')' ';'
    |
    FOR '('  for_specifier stmt 
    |
    PARDO '(' IDENT ':' expr ')' stmt
    ;

for_specifier:
    first_for_item expr ';' maybe_expr ')'
    |
    error ')'
    ;

first_for_item: stmt_expr | variable_declaration;

stmt_jump: 
    BREAK 
    | 
    CONTINUE 
    | 
    RETURN maybe_expr
    ;

maybe_expr: %empty {$$=NULL;} | expr{$$=$1;};
