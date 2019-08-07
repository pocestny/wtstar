%code top{
  #define __PARSER_UTILS__
}

%code requires {
  #include <math.h>
  #include <string.h>
  #include <stdarg.h>
  #include <stdlib.h>
  #include "ast.h"
}

%debug
%defines
%locations
%define parse.error verbose
%param {ast_t *ast}
%define api.pure full

%code provides {
   #define YY_DECL \
       int yylex(YYSTYPE * yylval_param, YYLTYPE * yylloc_param, ast_t* ast)
   YY_DECL;
  void yyerror (YYLTYPE *yylloc, ast_t *, char const *, ...);
  #include "parser_utils.c"

}

%initial-action {
  add_basic_types(ast);
}

%define api.token.prefix {TOK_} 
%union
{
  int32_t                 int_val;
  float                   float_val;
  char                    char_val;
  char*                   string_val;
  ast_node_t*             ast_node_val;
  static_type_member_t *  static_type_member_val;
  static_type_t *         static_type_val;
}

%token <int_val>          INT_LITERAL    
%token <float_val>        FLOAT_LITERAL  
%token <char_val>         CHAR_LITERAL   
%token <string_val>       STRING_LITERAL IDENT 
%token <static_type_val>  TYPENAME

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
%destructor { if ($$) ast_node_t_delete($$); $$=NULL;} <ast_node_val>
%destructor { if ($$) static_type_member_t_delete($$); $$=NULL;} <static_type_member_val>



%type <int_val> 
    placeholder_list 
    array_dim_spec
    assign_operator eq_operator rel_operator add_operator mult_operator unary_operator

%type <string_val>
    open_function

%type <ast_node_val> 
    expr expr_assign expr_or expr_and expr_eq expr_rel expr_add expr_mult
    expr_pow expr_cast expr_unary expr_postfix expr_primary expr_literal
    initializer_list initializer_item
    expr_list maybe_expr
    input_variable_declaration variable_declaration variable_declarator_list
    variable_init_declarator variable_declarator input_variable_declarator
    static_variable_declarator input_variable_declarator_list
    ranges_list range
    nonempty_parameter_declarator_list parameter_declarator_list maybe_body
    parameter_declarator
    stmt_scope 


%type <static_type_member_val>
    typedef_list typedef_item typedef_ident_list
%%
  
  /* ************************* PROGRAM ***************************** 
      the golbal structure of a program (includes are handled in the lexer)
  */

program: program_item | program program_item;

program_item 
             : typedef 
             | variable_declaration { if (!append_variables(ast,$1)) YYERROR; }
             | input_declaration 
             | output_declaration  
             | alias_declaration   
             | function_declaration 
             | stmt
             ;

  
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

typedef 
       : TYPE IDENT '{' typedef_list '}' {if (!make_typedef(ast,&@$,$2,&@2,$4)) YYERROR;}
       | TYPE error '}'
       ;

typedef_list
            : typedef_item {$$=$1;} 
            | typedef_list typedef_item {$$=$1; append(static_type_member_t,&$$,$2);}

typedef_item
            : TYPENAME typedef_ident_list ';' 
                {
                  $$=$2;
                  list_for(tt,static_type_member_t,$$) 
                    tt->type=$1;
                  list_for_end
                }
            | TYPENAME error ';' {$$=NULL;}
            ;


typedef_ident_list: 
    IDENT { $$=static_type_member_t_new($1,NULL); free($1); }
    | 
    typedef_ident_list ',' IDENT 
        {
          if (static_type_member_find($1,$3)) {
            yyerror(&@3,ast,"duplicate type member %s",$3);
            free($3);
            static_type_member_t_delete($1);            
            $$=NULL;
            YYERROR;
          }
          $$=$1;
          append(static_type_member_t,&$$,static_type_member_t_new($3,NULL));
          free($3);
        }
    ;

  
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

input_declaration 
                 : INPUT  input_variable_declaration 
                    {
                      add_variable_flag(IO_FLAG_IN,$2);
                      if (!append_variables(ast,$2)) YYERROR;
                    }
                 | INPUT error ';' 
                 ;

output_declaration
                  : OUTPUT variable_declaration 
                    {
                      add_variable_flag(IO_FLAG_OUT,$2);
                      if (!append_variables(ast,$2)) YYERROR;
                    }
                  | OUTPUT error ';'
                  ;

alias_declaration
                 : ALIAS IDENT '=' IDENT "[[" ranges_list "]]" ';'
                    {
                      if (!init_alias(ast,&@2,$2,&@4,$4,$6)) YYERROR;
                    }
                 | ALIAS error ';' 
                 ;


ranges_list: 
    range {$$=$1;}
    |
    ranges_list ',' range {$$=$1;append(ast_node_t,&$$,$3);}
    ;

range: 
    expr_assign ':' expr_assign {$$=$1;append(ast_node_t,&$$,$3);}
    | 
    expr_assign 
      {
        $$=$1;
        append(ast_node_t,&$$,ast_node_t_new(&@1,AST_NODE_EXPRESSION,EXPR_EMPTY));
      }
    ;


  /* possibly declare several variables of the same type */
variable_declaration: 
    TYPENAME variable_declarator_list ';' 
      {
        $$=$2;
        list_for(v,ast_node_t,$$) 
          v->val.v->base_type=$1;  
        list_for_end
      }
    |
    TYPENAME error ';' {$$=NULL;}
    ;

variable_declarator_list: 
    variable_init_declarator {$$=$1;}
    | 
    variable_declarator_list ',' variable_init_declarator 
      {
        $$=$1;
        append(ast_node_t,&$$,$3); 
      }
    | 
    error ','  variable_init_declarator {$$=$3;}
    ;


  /* declare variable, and possibly initialize it */
variable_init_declarator: 
    variable_declarator {$$=$1;}
    | 
    variable_declarator '=' initializer_item 
      { 
        $$=$1; 
        $$->val.v->initializer = $3;
      }
    ;

variable_declarator: 
    static_variable_declarator {$$=$1;}
    | 
    static_variable_declarator '[' expr_list ']'
      {
        $$=$1;
        if (!init_array(ast,$1,$3)) YYERROR;
      }
    ;


  /*  ***************************************
      input variables are handled differently

  */
input_variable_declaration: 
    TYPENAME input_variable_declarator_list ';' 
      {
        $$=$2;
        list_for(v,ast_node_t,$$) 
          v->val.v->base_type=$1;  
        list_for_end
      }
    |
    TYPENAME error ';' {$$=NULL;}
    ;

input_variable_declarator_list: 
    input_variable_declarator {$$=$1;}
    | 
    input_variable_declarator_list ',' input_variable_declarator 
      {
        $$=$1;
        append(ast_node_t,&$$,$3); 
      }
    | 
    error ',' input_variable_declarator {$$=$3;}
    ;

input_variable_declarator: 
    static_variable_declarator {$$=$1;}
    | 
    static_variable_declarator '[' placeholder_list ']' 
      {
        $$=$1;
        if (!init_input_array(ast,$1,$3)) YYERROR;
      }
    ;


placeholder_list: 
    "_" {$$=1;}
    |
    placeholder_list ',' "_" {$$=$1+1;}
    ;


  /*
      end of input variables specific part
      ***************************************
  */

static_variable_declarator: IDENT 
                            {
                                $$=init_variable(ast,&@1,$1); 
                                if (!$$) YYERROR;
                            }



  /* ******************************* FUNCTIONS  *********************************** 
  
      functions are declared in C-like style

      <output_type> <function_name> ( <param_1> , .... , <param_n> ) { <body> }

      it is possible to have separate declaration    
  
      <output_type> <function_name> ( <param_1> , .... , <param_n> ) ;
  
  */


function_declaration: open_function maybe_body {add_function_scope(ast,$1,$2);}

open_function:
    TYPENAME IDENT '(' parameter_declarator_list ')'
      {
        if (!define_function(ast,&@$,$1,$2,&@2,$4))
          YYERROR;
        else $$=$2;
      }
    |
    error ')' {$$=NULL;}
    ;


maybe_body: 
    ';' {$$=NULL;}
    | 
    stmt_scope {
      $$=$1;
      // remove from current scope
      unchain_last(&ast->current_scope->items);
    }
    ;

parameter_declarator_list: 
    %empty {$$=NULL;}
    |  
    nonempty_parameter_declarator_list {$$=$1;}
    ;

nonempty_parameter_declarator_list: 
    parameter_declarator {$$=$1;}
    | 
    nonempty_parameter_declarator_list ',' parameter_declarator
      {
        $$=$1;
        if ($3) append(ast_node_t,&$$,$3);
      }
    ;

parameter_declarator 
                    : TYPENAME IDENT array_dim_spec 
                    {
                       $$ = init_variable(ast,&@2,$2);
                       $$->val.v->base_type=$1;
                       $$->val.v->num_dim=$3;
                    };

 
array_dim_spec: %empty {$$=0;} | '[' placeholder_list ']' {$$=$2;};                                  
  
  
  /* ************************* EXPRESSIONS  ***************************** */

expr: 
    expr_assign {$$=$1;} 
    |  
    expr ',' expr_assign {$$=$1;append(ast_node_t,&$$,$3);}
    ;

expr_assign:
    expr_or {$$=$1;} 
    | 
    expr_unary  assign_operator expr_assign 
      {
        $$ = ast_node_t_new(&@$,AST_NODE_EXPRESSION,EXPR_BINARY,$1,$2,$3);
        if (!fix_expression_type($$))
          YYERROR;
      }
    ;

expr_or: 
    expr_and {$$=$1;} 
    | 
    expr_or OR expr_and
      {
        $$ = ast_node_t_new(&@$,AST_NODE_EXPRESSION,EXPR_BINARY,$1,TOK_OR,$3);
        if (!fix_expression_type($$))
          YYERROR;
      }
    ;

expr_and: 
    expr_eq {$$=$1;} 
    | 
    expr_and AND expr_eq
      {
        $$ = ast_node_t_new(&@$,AST_NODE_EXPRESSION,EXPR_BINARY,$1,TOK_AND,$3);
        if (!fix_expression_type($$))
          YYERROR;
      }
    ;


expr_eq:  
    expr_rel {$$=$1;} 
    | 
    expr_eq  eq_operator expr_rel
      {
        $$ = ast_node_t_new(&@$,AST_NODE_EXPRESSION,EXPR_BINARY,$1,$2,$3);
        if (!fix_expression_type($$))
          YYERROR;
      }
    ;


expr_rel:  
    expr_add {$$=$1;} 
    | 
    expr_rel  rel_operator  expr_add
      {
        $$ = ast_node_t_new(&@$,AST_NODE_EXPRESSION,EXPR_BINARY,$1,$2,$3);
        if (!fix_expression_type($$))
          YYERROR;
      }
    ;


expr_add: 
    expr_mult {$$=$1;} 
    |
    expr_add expr_literal
      {
        $$ = ast_node_t_new(&@$,AST_NODE_EXPRESSION,EXPR_BINARY,$1,'+',$2);
        if (!fix_expression_type($$))
          YYERROR;
      }
    |
    expr_add    add_operator    expr_mult
      {
        $$ = ast_node_t_new(&@$,AST_NODE_EXPRESSION,EXPR_BINARY,$1,$2,$3);
        if (!fix_expression_type($$))
          YYERROR;
      }
    ;

expr_mult: 
    expr_pow {$$=$1;} 
    | 
    expr_mult   mult_operator   expr_pow
      {
        $$ = ast_node_t_new(&@$,AST_NODE_EXPRESSION,EXPR_BINARY,$1,$2,$3);
        if (!fix_expression_type($$))
          YYERROR;
      }
    ;


expr_pow:  
    expr_cast {$$=$1;} 
    | 
    expr_cast '^' expr_pow
      {
        $$ = ast_node_t_new(&@$,AST_NODE_EXPRESSION,EXPR_BINARY,$1,'^',$3);
        if (!fix_expression_type($$))
          YYERROR;
      }
    ;


expr_cast: 
    expr_unary {$$=$1;} 
    | 
    '(' TYPENAME ')'  expr_cast
      {
        $$ = ast_node_t_new(&@$,AST_NODE_EXPRESSION,EXPR_CAST,$2,$4);
        if (!fix_expression_type($$))
          YYERROR;
      }
    | 
    '(' TYPENAME ')' '{' initializer_list '}'
      {
         $$ = ast_node_t_new(&@$,AST_NODE_EXPRESSION,EXPR_CAST,$2,$5);
        if (!fix_expression_type($$))
          YYERROR;
       }
    ;

expr_unary:   
    expr_postfix {$$=$1;}  
    | 
    unary_operator expr_postfix
      {
         $$ = ast_node_t_new(&@$,AST_NODE_EXPRESSION,EXPR_PREFIX,$1,$2);
        if (!fix_expression_type($$))
          YYERROR;
      }
    ;


expr_postfix: 
    expr_primary  {$$=$1;}
    | 
    expr_postfix '.' IDENT
      {
        $$ = create_specifier_expr(&@$,ast,$1,$3,&@3);
        if (!$$) YYERROR;
      }
    |
    expr_postfix INC 
      {
         $$ = ast_node_t_new(&@$,AST_NODE_EXPRESSION,EXPR_POSTFIX,$1,TOK_INC);
        if (!fix_expression_type($$))
          YYERROR;
      }
    | 
    expr_postfix DEC
      {
         $$ = ast_node_t_new(&@$,AST_NODE_EXPRESSION,EXPR_POSTFIX,$1,TOK_DEC);
        if (!fix_expression_type($$))
          YYERROR;
      }
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
    | '-' {$$='-';}
    | INC {$$=TOK_INC;}
    | DEC {$$=TOK_DEC;}
    ;
  

expr_primary: 
    IDENT  
      {
        $$=expression_variable(ast,&@1,$1);
      }
    |
    IDENT SIZE 
      {
        $$=expression_variable(ast,&@1,$1);
        $$->val.e->variant=EXPR_SIZEOF;
        $$->val.e->val.v->params=expression_int_val(0);
        if ($$->val.e->val.v->var->num_dim==0) {
          yyerror(&@1,ast,"sizeof appied to non-array");
          ast_node_t_delete($$);
          YYERROR;
        }
      }
    |
    IDENT SIZE "(" expr_assign ")" 
      {
        $$=expression_variable(ast,&@1,$1);
        $$->val.e->variant=EXPR_SIZEOF;
        $$->val.e->val.v->params=$4;
        if ($$->val.e->val.v->var->num_dim==0) {
          yyerror(&@1,ast,"sizeof appied to non-array");
          ast_node_t_delete($$);
          YYERROR;
        } else if (!expr_int($4->val.e)) {
          yyerror(&@1,ast,"non integral array dimension");
          ast_node_t_delete($$);
          YYERROR;
        }
      }
    |
    IDENT "[["  ranges_list "]]"  
      {
        $$=expression_variable(ast,&@1,$1);
        if ($$) {
          if (!add_expression_array_parameters($$,$3,1))
            YYERROR;
        }
        else {
          ast_node_t_delete($3);
          YYERROR;
        }
      }
    |
    IDENT '[' expr_list ']' 
      {
        $$=expression_variable(ast,&@1,$1);
        if ($$) {
          if (!add_expression_array_parameters($$,$3,0))
            YYERROR;
        }
        else {
          ast_node_t_delete($3);
          YYERROR;
        }
      }
    | 
    IDENT '(' expr_list ')' 
      {
        $$=expression_call(ast,&@$,$1,$3);
        if (!$$) YYERROR;
      }
    | 
    IDENT '(' ')' 
      {
        $$=expression_call(ast,&@$,$1,NULL);
        if (!$$) YYERROR;
      }
    | 
    '(' expr ')' {$$=$2;}
    |
    expr_literal {$$=$1;}
    ;


expr_literal: 
    INT_LITERAL
      {
        $$=ast_node_t_new(NULL, AST_NODE_EXPRESSION, EXPR_LITERAL);
        $$->val.e->type->type = __type__int->val.t;
        $$->val.e->val.l = malloc(sizeof(int));
        *(int *)($$->val.e->val.l) = $1;
      }
    |
    FLOAT_LITERAL 
      {
        $$=ast_node_t_new(NULL, AST_NODE_EXPRESSION, EXPR_LITERAL);
        $$->val.e->type->type = __type__float->val.t;
        $$->val.e->val.l = malloc(sizeof(float));
        *(float *)($$->val.e->val.l) = $1;
      }
    | 
    CHAR_LITERAL 
      {
        $$=ast_node_t_new(NULL, AST_NODE_EXPRESSION, EXPR_LITERAL);
        $$->val.e->type->type = __type__char->val.t;
        $$->val.e->val.l = malloc(1);
        *(char *)($$->val.e->val.l) = $1;
      }
    ;


  /* bracketed comma separated initializer list - returns expression */
initializer_list 
                : initializer_item {$$=$1;} 
                | STRING_LITERAL 
                  {
                    $$=ast_node_t_new(&@1,AST_NODE_EXPRESSION,EXPR_INITIALIZER);
                    $$->val.e->type->compound=1;
                    $$->val.e->type->list=NULL;
                    for(int i=strlen($1)-1;i>=0;--i) {
                        ast_node_t *tmp =
                              ast_node_t_new(NULL, AST_NODE_EXPRESSION, EXPR_LITERAL);
                        tmp->val.e->type->type = __type__char->val.t;
                        tmp->val.e->val.l = malloc(1);
                        *(char *)(tmp->val.e->val.l) = $1[i];
                        tmp->next=$$->val.e->val.i;
                        $$->val.e->val.i=tmp;

                        ALLOC_VAR(t,inferred_type_t);
                        t->compound=0;
                        t->type=__type__char->val.t;

                        inferred_type_item_t *tt= inferred_type_item_t_new(t);
                        tt->next=$$->val.e->type->list;
                        $$->val.e->type->list=tt;
                    }
                  }
                | initializer_list ',' initializer_item 
                  {
                    $$=$1;
                    inferred_type_append($$->val.e->type,$3->val.e->type);
                    append(ast_node_t,&($$->val.e->val.i),$3->val.e->val.i);
                    free($3);
                  }
                | error ',' initializer_item {$$=$3;}
                ;

initializer_item 
                : expr_assign 
      {
        if (!$1) $$=NULL;
        else {
          $$=ast_node_t_new(&@$,AST_NODE_EXPRESSION,EXPR_INITIALIZER);
          $$->val.e->type = inferred_type_copy($1->val.e->type);
          $$->val.e->val.i = $1;
        }
      }
      | '{' initializer_list '}' 
      {
        inferred_type_item_t *t =inferred_type_item_t_new($2->val.e->type);
        
        ALLOC_VAR(nt,inferred_type_t);

        $2->val.e->type = nt;
        $2->val.e->type->compound = 1;
        $2->val.e->type->list = t;
        $$ = $2;
      };

  /* list of expressions */
expr_list: 
    expr_assign {$$=$1;}
    | 
    expr_list ',' expr_assign 
      {
        $$=$1;
        append(ast_node_t,&$$,$3);
      }
    |
    error ',' expr_assign {$$=$3;}
    ;


  /* ************************* STATEMENTS  ***************************** 
  
  STMT_COND:
    par[0] = expression
    following two nodes are "then" "else" statements

  STMT_WHILE and STMT_DO
    par[0] = expression
    next node is the statement

  STMT_FOR:
    par[0] = encapsulating scope 
             contains 4 nodes: three sections from the "for" definition 
             and one resulting statement

  STMT_PARDO:
    par[0] = scope with first item the driving variable
             contains the statement
    par[1] = expression

  STMT_RETURN:
    par[0] = expression

  */

stmt: stmt_scope {ignore($1);}| stmt_expr  | stmt_cond | stmt_iter | stmt_jump ';' | error ';';

stmt_scope
            : '{' 
            {
              $<ast_node_val>$= ast_node_t_new(&@$,AST_NODE_SCOPE,ast->current_scope);
              ast->current_scope=$<ast_node_val>$->val.sc;
            } 
            scope_item_list '}'
            {
              $$=$<ast_node_val>2;
              ast->current_scope=$$->val.sc->parent;
              append(ast_node_t,&ast->current_scope->items,$$);
            }
  
            | '{' '}' {$$=NULL;}
          ;

scope_item_list:  scope_item | scope_item_list scope_item ;
scope_item 
          : variable_declaration  { if (!append_variables(ast,$1)) YYERROR; }
          | stmt;

stmt_expr 
         : expr ';' 
         {
            append(ast_node_t,&ast->current_scope->items,$1);
         }
    ;

stmt_cond 
         : IF '(' expr ')' 
          {
            ast_node_t *n =ast_node_t_new(&@$,AST_NODE_STATEMENT,STMT_COND);
            n->val.s->par[0]=$3;
            append(ast_node_t,&ast->current_scope->items,n);
          }
          stmt maybe_else 
         ;

maybe_else 
          : %empty  
            {
                append(ast_node_t,&ast->current_scope->items,
                      ast_node_t_new(&@$,AST_NODE_EXPRESSION,EXPR_EMPTY));
            }
          | ELSE stmt ;

stmt_iter
         : WHILE '(' expr ')' 
          {
            ast_node_t *n =ast_node_t_new(&@$,AST_NODE_STATEMENT,STMT_WHILE);
            n->val.s->par[0]=$3;
            append(ast_node_t,&ast->current_scope->items,n);
          }
          stmt
         | DO 
          {
            $<ast_node_val>$ =ast_node_t_new(&@$,AST_NODE_STATEMENT,STMT_DO);
            append(ast_node_t,&ast->current_scope->items,$<ast_node_val>$);
          }
          stmt WHILE '(' expr ')' ';'
          {
            $<ast_node_val>2->val.s->par[0]=$6;
          }
         |  FOR '('  
            {
              $<ast_node_val>$= ast_node_t_new(&@$,AST_NODE_SCOPE,ast->current_scope);
              ast->current_scope=$<ast_node_val>$->val.sc;
            } 
            for_specifier for_stmt 
            {
              ast_node_t *n =ast_node_t_new(&@$,AST_NODE_STATEMENT,STMT_FOR);
              ast->current_scope=$<ast_node_val>3->val.sc->parent;
              n->val.s->par[0]=$<ast_node_val>3;
              append(ast_node_t,&ast->current_scope->items,n);
            }
         | PARDO '(' IDENT ':'
            {
              $<ast_node_val>$= ast_node_t_new(&@$,AST_NODE_SCOPE,ast->current_scope);
              ast->current_scope=$<ast_node_val>$->val.sc;

              ast_node_t *v = init_variable(ast,&@3,$3);
              v->val.v->base_type=__type__int->val.t;
              append_variables(ast,v);
            } 
            expr ')' stmt
            {
              ast_node_t *n =ast_node_t_new(&@$,AST_NODE_STATEMENT,STMT_PARDO);
              ast->current_scope=$<ast_node_val>5->val.sc->parent;
              n->val.s->par[0]=$<ast_node_val>5;
              n->val.s->par[1]=$6;
              append(ast_node_t,&ast->current_scope->items,n);
            }
         ;

for_stmt: ';' | stmt;

for_specifier
             : first_for_item expr ';' maybe_expr ')'
                {
                  append(ast_node_t,&ast->current_scope->items,$2);
                  if ($4) {
                    append(ast_node_t,&ast->current_scope->items,$4);
                  } else {
                    append(ast_node_t,&ast->current_scope->items,
                        ast_node_t_new(&@$,AST_NODE_EXPRESSION,EXPR_EMPTY));
                  }
                }
             | error ')' 
             ;

first_for_item 
              : stmt_expr 
              | variable_declaration  
                { 
                  if (!append_variables(ast,$1)) YYERROR; 
                }
              | ';' 
                {
                  append(ast_node_t,&ast->current_scope->items,
                    ast_node_t_new(&@$,AST_NODE_EXPRESSION,EXPR_EMPTY)
                  );
                }  

stmt_jump
         : BREAK 
          {
            ast_node_t * n=ast_node_t_new(&@$,AST_NODE_STATEMENT,STMT_BREAK);
            append(ast_node_t,&ast->current_scope->items,n);
          }
         | CONTINUE 
          {
            ast_node_t *n =ast_node_t_new(&@$,AST_NODE_STATEMENT,STMT_CONTINUE);
            append(ast_node_t,&ast->current_scope->items,n);
          }
         | RETURN maybe_expr
          {
            ast_node_t * n=ast_node_t_new(&@$,AST_NODE_STATEMENT,STMT_RETURN);
            n->val.s->par[0]=$2;
            append(ast_node_t,&ast->current_scope->items,n);
          }
         ;

maybe_expr: %empty {$$=NULL;} | expr{$$=$1;};
