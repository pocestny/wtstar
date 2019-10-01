#include <stdlib.h>
#include <string.h>

#include <code_generation.h>
#include <driver.h>
#include <errors.h>
#include <vm.h>

static ast_t *ast = NULL;
static writer_t *code = NULL;
static writer_t *outw = NULL;
static virtual_machine_t *env = NULL;

int web_W() { return (env) ? env->W : 0; }
int web_T() { return (env) ? env->T : 0; }

int web_compile(char *name, char *text) {
  delete_errors();
  if (ast) ast_t_delete(ast);
  if (code) writer_t_delete(code);

  code = writer_t_new(WRITER_STRING);

  driver_init();
  driver_set_file(name, text);
  ast = driver_parse(name);

  if (!ast->error_occured) ast->error_occured = emit_code(ast, code, 0);

  driver_destroy();
  return ast->error_occured;
}

int web_start(char *input) {
  if (!code) return 1;

  if (outw) writer_t_delete(outw);
  outw = writer_t_new(WRITER_STRING);
  if (env) virtual_machine_t_delete(env);
  delete_errors();
  env = virtual_machine_t_new((uint8_t *)(code->str.base), code->str.ptr);

  reader_t *r = reader_t_new(READER_STRING, input);
  read_input(r, env);
  reader_t_delete(r);

  return 0;
}

int web_run(int limit) {
  int res = execute(env, limit, 0, 0);
  if (res == 0)
    for (int i = 0; i < env->n_out_vars; i++) {
      write_output(outw, env, i);
      out_text(outw, "\n");
    }
  return res;
}

char *web_output() {
  if (outw) return outw->str.base;
  return NULL;
}
