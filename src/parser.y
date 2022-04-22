%code top{
  #define __PARSER_UTILS__
}

%code requires {
  #include <math.h>
  #include <string.h>
  #include <stdarg.h>
  #include <stdlib.h>

  #include <ast.h>
  #include <code.h>

  typedef void* yyscan_t;
}

%debug
%defines
%locations
%define parse.error verbose
%param {ast_t *ast}
%lex-param {yyscan_t scanner}
%parse-param {yyscan_t scanner}
%define api.pure full

%code provides {
   #define YY_DECL \
       int yylex(YYSTYPE * yylval_param, YYLTYPE * yylloc_param, ast_t* ast, yyscan_t yyscanner)
   YY_DECL;
  void yyerror (YYLTYPE *yylloc_param, ast_t *, char const *, ...);
  #include <parser_utils.c>

}

%initial-action {
  add_basic_types(ast);
  add_builtin_functions(ast);
  #define MSG(...) fprintf(stderr,__VA_ARGS__)
  ast->mem_mode=TOK_MODE_CREW;
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

%token <int_val>          INT_LITERAL  BREAKPOINT  
%token <float_val>        FLOAT_LITERAL  
%token <char_val>         CHAR_LITERAL   
%token <string_val>       STRING_LITERAL IDENT 
%token <static_type_val>  TYPENAME

%token TYPE INPUT OUTPUT IF ELSE FOR WHILE PARDO DO RETURN SIZE DIM SORT MODE_CREW MODE_EREW MODE_CCRCW

%token < > EQ "=="
%token < > NEQ "!="
%token < > LEQ "<="
%token < > GEQ ">="
%token < > AND "&&"
%token < > OR "||"

%token < > LAST_BIT "~|"

%token < > INC "++"
%token < > DEC "--"

%token < > PLUS_ASSIGN "+="
%token < > MINUS_ASSIGN "-="
%token < > TIMES_ASSIGN "*="
%token < > DIV_ASSIGN "/="
%token < > MOD_ASSIGN "%="

%token < > DONT_CARE "_"


%destructor { if ($$) free($$); $$=NULL;} <string_val>
%destructor { 
if ($$) {
  if (
    ($$->node_type == AST_NODE_SCOPE && ast->current_scope == $$->val.sc) ||
    ($$->node_type == AST_NODE_STATEMENT && 
      ($$->val.s->variant == STMT_COND || $$->val.s->variant == STMT_WHILE ||
        $$->val.s->variant == STMT_DO
      ) &&
      ast->current_scope == $$->val.s->par[1]->val.sc 
    )
    )  ast->current_scope=ast->current_scope->parent;
    ast_node_t_delete($$); 
  }
  $$=NULL;
} <ast_node_val>
%destructor { if ($$) static_type_member_t_delete($$); $$=NULL;} <static_type_member_val>

%destructor {$$=NULL;} input_variable_declaration variable_declaration variable_declarator_list
    variable_init_declarator variable_declarator input_variable_declarator
    static_variable_declarator input_variable_declarator_list open_function

%type <int_val> 
    placeholder_list 
    array_dim_spec
    assign_operator eq_operator rel_operator add_operator mult_operator unary_operator

%type <ast_node_val> 
    expr expr_assign expr_or expr_and expr_eq expr_rel expr_add expr_mult
    expr_pow expr_cast expr_unary expr_postfix expr_primary expr_literal
    initializer_list initializer_item
    expr_list maybe_expr
    input_variable_declaration variable_declaration variable_declarator_list
    variable_init_declarator variable_declarator input_variable_declarator
    static_variable_declarator input_variable_declarator_list
    nonempty_parameter_declarator_list parameter_declarator_list 
    parameter_declarator
    stmt_scope open_function
    specifier_list

%type <static_type_val> type_decl


%type <static_type_member_val>
    typedef_list typedef_item typedef_ident_list
%%
  
  /* ************************* PROGRAM ***************************** 
      the golbal structure of a program (includes are handled in the lexer)
  */

program: program_item | program program_item;

program_item 
             :  typedef 
             | variable_declaration {ignore($1);} 
             |  input_declaration 
             |  output_declaration  
             |  function_declaration 
             |  stmt
             | MODE_EREW {ast->mem_mode=TOK_MODE_EREW;}
             | MODE_CREW {ast->mem_mode=TOK_MODE_CREW;}
             | MODE_CCRCW {ast->mem_mode=TOK_MODE_CCRCW;}
             ;

  
  /* ******************************* TYPES *********************************** 

      Handles the definition of new types. 
      
      Types are recursive structs of basic types (int,float,char), i.e. array cannot
      be part of a type.
      
      type <new_type_name> {
        <type_1> <name_1_1>, ... ,<name_1_n1>;
        <type_2> <name_2_1>, ... ,<name_2_n2>;
        ...
      };

   */

typedef 
       : TYPE IDENT '{' typedef_list '}' {make_typedef(ast,&@$,$2,&@2,$4);$2=NULL;}
       | TYPE error '}' 
       ;

typedef_list
            : typedef_item {$$=$1;} 
            | typedef_list typedef_item {
              if ($2==NULL) $$=$1;
              else if ($1==NULL) $$=$2;
              else {
                $$=$1; 
                list_append(static_type_member_t,&$$,$2);
              }
            }

typedef_item
            : TYPENAME typedef_ident_list ';' 
                {
                  $$=$2;
                  if ($$) {
                    list_for(tt,static_type_member_t,$$) 
                      tt->type=$1;
                    list_for_end
                  }
                }
            | TYPENAME error ';' {$$=NULL;}
            ;


typedef_ident_list: 
    error ',' IDENT  { $$=static_type_member_t_new($3,NULL); free($3); $3=NULL;}
    |
    IDENT { $$=static_type_member_t_new($1,NULL); free($1); $1=NULL;}
    | 
    typedef_ident_list ',' IDENT 
        {
          $$=$1;
          if (static_type_member_find($1,$3)) {
            yyerror(&@3,ast,"duplicate type member '%s' in type definition",$3);
          } else {
            static_type_member_t *nt = static_type_member_t_new($3,NULL);
            list_append(static_type_member_t,&$$,nt);
          }
          free($3);
          $3=NULL;
        }
    ;

  
  /* ******************************* VARIABLES  ***********************************
 
      variables can be of static type (basic or defined), arrays, or alises

      <type> <new_name>;
      <type> <new_name> = <value> ;
      
      <type> <new_name> [<size_1>,<size_2>,...,<size_d>];
      
      input arrays don't specify sizes, only dimensions:
      input <type> <name> [ _ , ... , _ ];

  */

input_declaration 
                 : INPUT  input_variable_declaration 
                    {
                      if ($2) add_variable_flag(IO_FLAG_IN,$2);
                    }
                 | INPUT error ';' 
                 ;

output_declaration
                  : OUTPUT variable_declaration 
                    {
                      if($2) add_variable_flag(IO_FLAG_OUT,$2);
                    }
                  | OUTPUT error ';'
                  ;


  /* possibly declare several variables of the same type */
variable_declaration: 
      type_decl variable_declarator_list ';' 
      {
        $$=$2;
      }
    |
    type_decl error ';' {$$=NULL;}
    ;

variable_declarator_list: 
    variable_init_declarator {$$=$1;}
    | 
    variable_declarator_list ',' variable_init_declarator 
      {
        $$=$1;
        if (!$$) $$=$3;
      }
    | 
    error ','  variable_init_declarator {$$=$3;}
    ;


  /* declare variable, and possibly initialize it */
variable_init_declarator: 
    variable_declarator {$$=$1;}
    | 
    variable_declarator '=' expr_assign
      { 
        $$=$1; 
        if ($$) {
          if ($3) {
            ast_node_t *nn =ast_node_t_new(&@3,AST_NODE_EXPRESSION,EXPR_INITIALIZER);
            nn->val.e->type = inferred_type_copy($3->val.e->type);
            nn->val.e->val.i = $3;
            $$->val.v->initializer = nn;
          }
        } else 
          ast_node_t_delete($3);
      }
    | variable_declarator '=' '{' initializer_list '}' 
    {
      $$=$1;
      if ($$) $$->val.v->initializer=$4;
      else ast_node_t_delete($4);
    }
    
    ;



variable_declarator: 
    static_variable_declarator {$$=$1;if ($$) $$->val.v->need_init=1;}
    | 
    static_variable_declarator '[' expr_list ']'
      {
        $$=$1;
        if ($$) init_array(ast,$1,$3);
        else ast_node_t_delete($3);
      }
    ;


  /*  ***************************************
      input variables are handled differently

  */
input_variable_declaration: 
    type_decl input_variable_declarator_list ';' 
      {
        $$=$2;
      }
    |
    type_decl error ';' {$$=NULL;}
    ;

input_variable_declarator_list: 
    input_variable_declarator {$$=$1;}
    | 
    input_variable_declarator_list ',' input_variable_declarator 
      {
        $$=$1;
        if (!$$) $$=$3;
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
        if ($$) init_input_array(ast,$1,$3);
      }
    | 
    static_variable_declarator '[' error ']' {
      yyerror(&@1,ast,"wrong list of placeholders");
      $$=$1;
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
                                if ($$) {
                                 append_variables(ast,$$);
                                 $$->val.v->base_type=ast->current_type;
                                }
                                $1=NULL;
                            }



  /* ******************************* FUNCTIONS  *********************************** 
  
      functions are declared in C-like style

      <output_type> <function_name> ( <param_1> , .... , <param_n> ) { <body> }

      it is possible to have separate declaration    
  
      <output_type> <function_name> ( <param_1> , .... , <param_n> ) ;
  
  */


function_declaration 
    : open_function ';'
    | open_function '{'
        {
            $<ast_node_val>$ = ast_node_t_new(&@$,AST_NODE_SCOPE,ast->current_scope);
            ast->current_scope=$<ast_node_val>$->val.sc;
            if ($1) {
               $1->val.f->root_scope = $<ast_node_val>$->val.sc;
               $<ast_node_val>$->val.sc->fn = $1->val.f;
               for (ast_node_t *p=$1->val.f->params;p;p=p->next)
                  p->val.v->scope = $<ast_node_val>$->val.sc;
            }
         } 
      maybe_scope_item_list '}'
         {
           ast->current_scope=$<ast_node_val>3->val.sc->parent;
           free($<ast_node_val>3);
         }


open_function:
    type_decl IDENT '(' parameter_declarator_list ')'
      {
        $$=define_function(ast,&@$,$1,$2,&@2,$4);
        $2=NULL;
      }
    |
    error ')' {$$=NULL;}
    ;

maybe_scope_item_list: %empty | scope_item_list | error; 

parameter_declarator_list: 
    %empty {$$=NULL;}
    |  
    { 
      // make new scope so there is no variable redefinition
      $<ast_node_val>$ = ast_node_t_new(&@$,AST_NODE_SCOPE,ast->current_scope);
      ast->current_scope=$<ast_node_val>$->val.sc;
    }
    nonempty_parameter_declarator_list 
    {
      $$=$2;
      ast->current_scope=$<ast_node_val>1->val.sc->parent;
      ast_node_t_delete($<ast_node_val>1);
    }
    ;

nonempty_parameter_declarator_list: 
    parameter_declarator {$$=$1;}
    | 
    nonempty_parameter_declarator_list ',' parameter_declarator
      {
        $$=$1;
        if ($3) {
          if (ast_node_find($$,$3->val.v->name))
            yyerror(&@3,ast,"redefinition of parameter");
          else
            list_append(ast_node_t,&$$,$3);
        }
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
type_decl: TYPENAME 
      {
        ast->current_type=$1;
        $$=$1;
      }
  
  /* ************************* EXPRESSIONS  ***************************** */

expr: 
    expr_assign {$$=$1;} 
    |  
    expr ',' expr_assign {$$=$1;list_append(ast_node_t,&$$,$3);}
    ;

expr_assign:
    expr_or {$$=$1;} 
    | 
    expr_unary  assign_operator expr_assign 
      {
        $$ = ast_node_t_new(&@$,AST_NODE_EXPRESSION,EXPR_BINARY,$1,$2,$3);
        if (!fix_expression_type(ast,&@$,$$)) {
          ast_node_t_delete($$);
          $$=NULL;
        }
      }
    ;

expr_or: 
    expr_and {$$=$1;} 
    | 
    expr_or OR expr_and
      {
        $$ = ast_node_t_new(&@$,AST_NODE_EXPRESSION,EXPR_BINARY,$1,TOK_OR,$3);
        if (!fix_expression_type(ast,&@$,$$)) {
          ast_node_t_delete($$);
          $$=NULL;
        }
      }
    ;

expr_and: 
    expr_eq {$$=$1;} 
    | 
    expr_and AND expr_eq
      {
        $$ = ast_node_t_new(&@$,AST_NODE_EXPRESSION,EXPR_BINARY,$1,TOK_AND,$3);
        if (!fix_expression_type(ast,&@$,$$)) {
          ast_node_t_delete($$);
          $$=NULL;
        }
      }
    ;


expr_eq:  
    expr_rel {$$=$1;} 
    | 
    expr_eq  eq_operator expr_rel
      {
        $$ = ast_node_t_new(&@$,AST_NODE_EXPRESSION,EXPR_BINARY,$1,$2,$3);
        if (!fix_expression_type(ast,&@$,$$)){
          ast_node_t_delete($$);
          $$=NULL;
        }
      }
    ;


expr_rel:  
    expr_add {$$=$1;} 
    | 
    expr_rel  rel_operator  expr_add
      {
        $$ = ast_node_t_new(&@$,AST_NODE_EXPRESSION,EXPR_BINARY,$1,$2,$3);
        if (!fix_expression_type(ast,&@$,$$)) {
          ast_node_t_delete($$);
          $$=NULL;
        }
      }
    ;


expr_add: 
    expr_mult {$$=$1;} 
    |
    expr_add    add_operator    expr_mult
      {
        $$ = ast_node_t_new(&@$,AST_NODE_EXPRESSION,EXPR_BINARY,$1,$2,$3);
        if (!fix_expression_type(ast,&@$,$$)) {
          ast_node_t_delete($$);
          $$=NULL;
        }
      }
    ;

expr_mult: 
    expr_pow {$$=$1;} 
    | 
    expr_mult   mult_operator   expr_pow
      {
        $$ = ast_node_t_new(&@$,AST_NODE_EXPRESSION,EXPR_BINARY,$1,$2,$3);
        if (!fix_expression_type(ast,&@$,$$)){
          ast_node_t_delete($$);
          $$=NULL;
        }
      }
    ;


expr_pow:  
    expr_cast {$$=$1;} 
    | 
    expr_cast '^' expr_pow
      {
        $$ = ast_node_t_new(&@$,AST_NODE_EXPRESSION,EXPR_BINARY,$1,'^',$3);
        if (!fix_expression_type(ast,&@$,$$)){
          ast_node_t_delete($$);
          $$=NULL;
        }
      }
    ;


expr_cast: 
    expr_unary {$$=$1;} 
    | 
    '(' TYPENAME ')'  expr_cast
      {
        $$ = ast_node_t_new(&@$,AST_NODE_EXPRESSION,EXPR_CAST,$2,$4);
        if (!fix_expression_type(ast,&@$,$$)) {
          ast_node_t_delete($$);
          $$=NULL;
        }
      }
    | 
    '(' TYPENAME ')' '{' initializer_list '}'
      {
         $$ = ast_node_t_new(&@$,AST_NODE_EXPRESSION,EXPR_CAST,$2,$5);
        if (!fix_expression_type(ast,&@$,$$)) {
          ast_node_t_delete($$);
          $$=NULL;
        }
     }
    ;

expr_unary:   
    expr_postfix {$$=$1;}  
    | 
    unary_operator expr_postfix
      {
        $$ = ast_node_t_new(&@$,AST_NODE_EXPRESSION,EXPR_PREFIX,$1,$2);
        if (!fix_expression_type(ast,&@$,$$)){
          ast_node_t_delete($$);
          $$=NULL;
        }
      }
    ;


expr_postfix: 
    expr_primary  {$$=$1;}
    |
    expr_postfix LAST_BIT {
        $$ = ast_node_t_new(&@$,AST_NODE_EXPRESSION,EXPR_POSTFIX,$1,TOK_LAST_BIT);
        if (!fix_expression_type(ast,&@$,$$)){
          ast_node_t_delete($$);
          $$=NULL;
        }
    }
    |
    expr_postfix '.' IDENT
      {
        $$=NULL;
        if($1) $$ = create_specifier_expr(&@$,ast,$1,$3,&@3);
      }
    |
    expr_postfix INC 
      {
        $$ = ast_node_t_new(&@$,AST_NODE_EXPRESSION,EXPR_POSTFIX,$1,TOK_INC);
        if (!fix_expression_type(ast,&@$,$$)){
          ast_node_t_delete($$);
          $$=NULL;
        }
      }
    | 
    expr_postfix DEC
      {
        $$ = ast_node_t_new(&@$,AST_NODE_EXPRESSION,EXPR_POSTFIX,$1,TOK_DEC);
        if (!fix_expression_type(ast,&@$,$$)) {
          ast_node_t_delete($$);
          $$=NULL;
        }
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
    | '|'{$$='|';}
    | '&'{$$='&';}
    | '~'{$$='~';}
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
        $1=NULL;
      }
    |
    IDENT DIM 
      {
        $$ = array_dimensions(ast,&@1,$1);
        $1=NULL;
      }
    |
    IDENT SIZE 
      {
        $$=expression_sizeof(ast,&@1,$1,NULL);
        $1=NULL;
      }
    |
    IDENT SIZE '(' expr_assign ')' 
      {
        $$=expression_sizeof(ast,&@1,$1,$4);
        $1=NULL;
      }
    |
    IDENT '[' expr_list ']' 
      {
        $$=expression_variable(ast,&@1,$1);
        if ($$) add_expression_array_parameters($$,$3);
        else {
          ast_node_t_delete($3);
        }
        $1=NULL;
      }
    | 
    SORT '(' IDENT ',' specifier_list ')' {
      $$ = expression_sort(ast,&@$,$3,$5);
      $3=NULL;
    }
    | 
    IDENT '(' expr_list ')' 
      {
        $$=expression_call(ast,&@$,$1,$3);
        $1=NULL;
      }
    | 
    IDENT '(' ')' 
      {
        $$=expression_call(ast,&@$,$1,NULL);
        $1=NULL;
      }
    | 
    '(' expr ')' {$$=$2;}
    |
    expr_literal {$$=$1;}
    ;

specifier_list:
              TYPENAME {
                $$=ast_node_t_new(&@$,AST_NODE_EXPRESSION,EXPR_EMPTY);
                $$->val.e->type->type=$1;
              }
              |
              specifier_list '.' IDENT {
                if (!$1) {
                  $$=NULL;
                  free($3);
                } else {
                  $$ = create_specifier_expr(&@$,ast,$1,$3,&@3);
                }
              }
              |
              error '.' IDENT {
                $$=NULL;
                free($3);
              }

expr_literal: 
    INT_LITERAL
      {
        $$=ast_node_t_new(&@$, AST_NODE_EXPRESSION, EXPR_LITERAL);
        $$->val.e->type->type = ast->__type__int->val.t;
        $$->val.e->val.l = malloc(sizeof(int));
        *((int *)($$->val.e->val.l)) = $1;
      }
    |
    FLOAT_LITERAL 
      {
        $$=ast_node_t_new(&@$, AST_NODE_EXPRESSION, EXPR_LITERAL);
        $$->val.e->type->type = ast->__type__float->val.t;
        $$->val.e->val.l = malloc(sizeof(float));
        *((float *)($$->val.e->val.l)) = $1;
      }
    | 
    CHAR_LITERAL 
      {
        $$=ast_node_t_new(&@$, AST_NODE_EXPRESSION, EXPR_LITERAL);
        $$->val.e->type->type = ast->__type__char->val.t;
        $$->val.e->val.l = malloc(1);
        *((char *)($$->val.e->val.l)) = $1;
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
                        tmp->val.e->type->type = ast->__type__char->val.t;
                        tmp->val.e->val.l = malloc(1);
                        *(char *)(tmp->val.e->val.l) = $1[i];
                        tmp->next=$$->val.e->val.i;
                        $$->val.e->val.i=tmp;

                        ALLOC_VAR(t,inferred_type_t);
                        t->compound=0;
                        t->type=ast->__type__char->val.t;

                        inferred_type_item_t *tt= inferred_type_item_t_new(t);
                        tt->next=$$->val.e->type->list;
                        $$->val.e->type->list=tt;
                    }
                  }
                | initializer_list ',' initializer_item 
                  {
                    $$=$1;
                    inferred_type_append($$->val.e->type,$3->val.e->type);
                    list_append(ast_node_t,&($$->val.e->val.i),$3->val.e->val.i);
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
        list_append(ast_node_t,&$$,$3);
      }
    |
    error ',' expr_assign {$$=$3;}
    ;


  /* ************************* STATEMENTS  ***************************** 
  

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
              list_append(ast_node_t,&ast->current_scope->items,$$);
            }
  
            | '{' '}' {$$=NULL;}
          ;

scope_item_list:  scope_item | scope_item_list scope_item ;
scope_item 
          : variable_declaration  
          | stmt;

stmt_expr 
         : expr ';' 
         {
            list_append(ast_node_t,&ast->current_scope->items,$1);
         }
    ;

stmt_cond 
         : IF '(' expr ')' 
          {
            ast_node_t *n =ast_node_t_new(&@$,AST_NODE_STATEMENT,STMT_COND);
            n->val.s->par[0]=$3;
            n->val.s->par[1]= ast_node_t_new(&@$,AST_NODE_SCOPE,ast->current_scope);
            ast->current_scope=n->val.s->par[1]->val.sc;
            $<ast_node_val>$ = n;  
          }
          stmt maybe_else {
            ast->current_scope=ast->current_scope->parent;
            list_append(ast_node_t,&ast->current_scope->items,$<ast_node_val>5);
          }
         ;

maybe_else 
          : %empty  
            {
                list_append(ast_node_t,&ast->current_scope->items,
                      ast_node_t_new(&@$,AST_NODE_EXPRESSION,EXPR_EMPTY));
            }
          | ELSE stmt ;

stmt_iter
         : WHILE '(' expr ')' 
          {
            ast_node_t *n =ast_node_t_new(&@$,AST_NODE_STATEMENT,STMT_WHILE);
            n->val.s->par[0]=$3;
            n->val.s->par[1]= ast_node_t_new(&@$,AST_NODE_SCOPE,ast->current_scope);
            ast->current_scope=n->val.s->par[1]->val.sc;
            $<ast_node_val>$ = n;  
          }
          stmt {
            ast->current_scope=ast->current_scope->parent;
            list_append(ast_node_t,&ast->current_scope->items,$<ast_node_val>5);
          }
         | DO 
          {
            ast_node_t *n =ast_node_t_new(&@$,AST_NODE_STATEMENT,STMT_DO);
            n->val.s->par[1]= ast_node_t_new(&@$,AST_NODE_SCOPE,ast->current_scope);
            ast->current_scope=n->val.s->par[1]->val.sc;
            $<ast_node_val>$ = n;  
          }
          stmt WHILE '(' expr ')' ';'
          {
            $<ast_node_val>2->val.s->par[0]=$6;
            ast->current_scope=ast->current_scope->parent;
            list_append(ast_node_t,&ast->current_scope->items,$<ast_node_val>2);
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
              list_append(ast_node_t,&ast->current_scope->items,n);
            }
         | PARDO '(' IDENT ':'
            {
              $<ast_node_val>$= ast_node_t_new(&@$,AST_NODE_SCOPE,ast->current_scope);
              ast->current_scope=$<ast_node_val>$->val.sc;

              ast_node_t *v = init_variable(ast,&@3,$3);
              v->val.v->base_type=ast->__type__int->val.t;
              append_variables(ast,v);
              $3=NULL;
            } 
            expr ')' stmt
            {
              ast_node_t *n =ast_node_t_new(&@$,AST_NODE_STATEMENT,STMT_PARDO);
              ast->current_scope=$<ast_node_val>5->val.sc->parent;
              n->val.s->par[0]=$<ast_node_val>5;
              n->val.s->par[1]=$6;
              list_append(ast_node_t,&ast->current_scope->items,n);
            }
         ;

for_stmt: ';' | stmt;

for_specifier
             : first_for_item expr ';' maybe_expr ')'
                {
                  list_append(ast_node_t,&ast->current_scope->items,$2);
                  if ($4) {
                    list_append(ast_node_t,&ast->current_scope->items,$4);
                  } else {
                    list_append(ast_node_t,&ast->current_scope->items,
                        ast_node_t_new(&@$,AST_NODE_EXPRESSION,EXPR_EMPTY));
                  }
                }
             | error ')' 
             ;

first_for_item 
              : stmt_expr 
              | variable_declaration  
              | ';' 
                {
                  list_append(ast_node_t,&ast->current_scope->items,
                    ast_node_t_new(&@$,AST_NODE_EXPRESSION,EXPR_EMPTY)
                  );
                }  

stmt_jump
         : 
         RETURN maybe_expr
          {
            ast_node_t * n=ast_node_t_new(&@$,AST_NODE_STATEMENT,STMT_RETURN);
            n->val.s->par[0]=$2;
            list_append(ast_node_t,&ast->current_scope->items,n);
            n->val.s->ret_fn=ast->current_scope->fn;
          }
          |
          BREAKPOINT {
            ast_node_t * n=ast_node_t_new(&@$,AST_NODE_STATEMENT,STMT_BREAKPOINT);
            n->val.s->tag=$1;
            n->val.s->par[0]=expression_int_val(ast, 1);
            list_append(ast_node_t,&ast->current_scope->items,n);
          }
          |
          BREAKPOINT '(' expr ')' {
            ast_node_t * n=ast_node_t_new(&@$,AST_NODE_STATEMENT,STMT_BREAKPOINT);
            n->val.s->tag=$1;
            n->val.s->par[0]=$3;
            list_append(ast_node_t,&ast->current_scope->items,n);
          }
         ;

maybe_expr: %empty {$$=NULL;} | expr{$$=$1;};
