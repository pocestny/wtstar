%{
#include <stdarg.h>

#include <parser.h>
#include <driver.h>
#include <errors.h>

int yycolumn = 1;

extern writer_t *driver_error_writer;

const char* const symbols[]={
  "(",")","{","}","[","]",
  ":",";",".",",",
  "<",">","!","~",
  "+","-","*","/", "%", "^","&","|","=",NULL};

const char* const symbols2[]={
  "==","!=","<=",">=",
  "&&","||","<<",">>","|~","~|",
  "++","--",
  "+=","-=","*=","/=","%=",NULL
  };

const int  values2[]={
TOK_EQ, TOK_NEQ, TOK_LEQ, TOK_GEQ,
TOK_AND,TOK_OR,TOK_SHL,TOK_SHR,TOK_FIRST_BIT,TOK_LAST_BIT,
TOK_INC,TOK_DEC,
TOK_PLUS_ASSIGN, TOK_MINUS_ASSIGN, TOK_TIMES_ASSIGN, TOK_DIV_ASSIGN, TOK_MOD_ASSIGN,
 -1};


#define YY_USER_ACTION  \
  yylloc->fl = yylloc->ll = yylineno;\
  yylloc->fc = yycolumn; yylloc->lc = yycolumn + yyleng - 1; \
  yylloc->fn = yylloc->ln = driver_current_file();\
  yycolumn += yyleng; \
  //printf("%3d %3d %3d |%s|\n",yylineno, yycolumn, yy_act, yytext);
%}

SPACE   [ \t]
LETTER  [a-zA-Z]
IDSYM   [a-zA-Z0-9_]
DIGIT   [0-9]
SYMBOL  [^a-zA-Z0-9 \t_'"]
SYMBOL2  [^a-zA-Z0-9 \t_'";]

%x include
%x comment
%option noyywrap
%option bison-bridge bison-locations

%%

  /* include directives are processed here */
#include{SPACE}*\"   {BEGIN(include);}

<include>[^\"]*\" {  
    yytext[yyleng-1]=0;
    driver_set_current_pos(yylineno,yycolumn);
    driver_push_file(yytext);
    yylineno=driver_current_line();
    yycolumn=driver_current_column();
    BEGIN(INITIAL);
  } 

  /* C-style comments */
"/*"                 {BEGIN(comment);}
<comment>"*/"      BEGIN(INITIAL);
<comment>[^*\n]+   // eat comment in chunks
<comment>"*"       // eat the lone star
<comment>\n        yylineno++; yycolumn=1;

  /* single line comment */
"//".*\n             yylineno++;yycolumn=1;

{DIGIT}+ { 
    yylval->int_val = atoi(yytext); 
    return TOK_INT_LITERAL; 
  }

{DIGIT}*\.{DIGIT}+  {
    yylval->float_val = atof(yytext); 
    return TOK_FLOAT_LITERAL; 
}

'.' {
    yylval->char_val = yytext[1]; 
    return TOK_CHAR_LITERAL;
  }

\"[^\"]*\" {
    yytext[yyleng-1]=0;
    yylval->string_val = strdup(yytext+1);
    return TOK_STRING_LITERAL;
  }


  /* keywords */

type/[^a-zA-Z0-9]      { return TOK_TYPE; }
input/[^a-zA-Z0-9]     { return TOK_INPUT; }
output/[^a-zA-Z0-9]    { return TOK_OUTPUT; }
if/[^a-zA-Z0-9]        { return TOK_IF; }
else/[^a-zA-Z0-9]      { return TOK_ELSE; }
for/[^a-zA-Z0-9]       { return TOK_FOR; }
while/[^a-zA-Z0-9]     { return TOK_WHILE; }
pardo/[^a-zA-Z0-9]       { return TOK_PARDO; }
do/[^a-zA-Z0-9]        { return TOK_DO; }
return/[^a-zA-Z0-9]    { return TOK_RETURN; }
\.{SPACE}*size/[^a-zA-Z0-9]      { return TOK_SIZE; }
dim/[^a-zA-Z0-9]       { return TOK_DIM; }

  /* not a keyword  */
[a-zA-Z_]{IDSYM}* {
  
  if (yytext[0]=='_'&&yyleng==1) 
  return TOK_DONT_CARE;

  ast_node_t *t=ast_node_find(ast->types,yytext);
  if (t) {
    yylval->static_type_val=t->val.t;
    return TOK_TYPENAME;
  }
  
  yylval->string_val = strdup(yytext);
  return TOK_IDENT; 
}


{SPACE}+  /* eat whitespace */
\n        yylineno++;yycolumn=1;

  /* symbols */

{SYMBOL2}{SYMBOL2} {
  for (int i=0;symbols2[i];i++) 
    if (!strcmp(symbols2[i],yytext)) 
    return values2[i];
  REJECT;  
}

{SYMBOL} {
  for (int i=0;symbols[i];i++) 
    if (!strcmp(symbols[i],yytext)) 
    return yytext[0];

  yyerror(yylloc,ast,"lexical error, unexpected token %s",yytext);
}



<<EOF>> {
    driver_set_current_pos(yylineno,yycolumn);
    if (!driver_pop_file()) {
      if (yy_buffer_stack) free(yy_buffer_stack);
      yy_buffer_stack=NULL;
      yyterminate();
    }
    else {
     yylineno = driver_current_line();
     yycolumn = driver_current_column();
     }
  }


%%


void yyerror(YYLTYPE *yylloc, ast_t *r,  const char  *s, ...) { 
  error_t *err = error_t_new();
  append_error_msg(err,"%s:%d:%d: ", yylloc->fn, yylloc->fl,  yylloc->fc);
  va_list args;
  int n;
  get_printed_length(s,n);
  va_start(args,s);
  append_error_vmsg(err,n,s,args);
  va_end(args);
  emit_error(err);
  r->error_occured=1;
}

