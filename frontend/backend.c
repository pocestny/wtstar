#include <string.h>
#include <stdlib.h>

#include <driver.h>
#include <errors.h>
#include <code_generation.h>
#include <runtime.h>
#include <vm.h>

static ast_t *ast = NULL;
static writer_t *code=NULL;
static char *ins=NULL;
static writer_t *outw=NULL;
static int W, T;

int EXEC_DEBUG=0;

int web_compile(char *text) {
  delete_errors();
  if (ast) ast_t_delete(ast);
  if (code) writer_t_delete(code);

  code=writer_t_new(WRITER_STRING);

  driver_init();
  driver_set_file("main",text);
  ast = driver_parse("main");
  driver_destroy();
  
  if (ast->error_occured) {
    error_t *err = error_t_new();
    append_error_msg(err,"there were errors");
    emit_error(err);
  } 
  else {
    emit_code(ast, code);
    error_t *err = error_t_new();
    append_error_msg(err,"compilation ok");
    emit_error(err);
  }

  return ast->error_occured;
}

int web_set_input(char *input) {
  if (ins) free(ins);
  ins=strdup(input);
  return 0;
}

int web_run() {
  if (outw) writer_t_delete(outw);
  outw = writer_t_new(WRITER_STRING);
  runtime_t *env = runtime_t_new((uint8_t*)(code->str.base), code->str.ptr);

  W=T=0;
  reader_t *r = reader_t_new(READER_STRING,ins);
  read_input(r,env);
  reader_t_delete(r);

  execute(env, &W, &T);
  write_output(outw,env);

  runtime_t_delete(env);
  return 0;
}

char * web_output() {
  if (outw) return outw->str.base;
  return NULL;
}
