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
static char *src_name = NULL;

/*
 * return code:
 */

#define WEB_NO_CODE -1
#define WEB_VM_READY 0
#define WEB_VM_RUNNING 1
#define WEB_VM_OK 2
#define WEB_VM_ERROR 3

static int __web_state = WEB_NO_CODE;

static uint64_t *tids = NULL;

typedef struct {
  char *type, *name, *dims, *value;
  int shared;
} var_data_t;

static var_data_t *var_data = NULL;
static int n_vars = 0;

int web_var_shared(int idx) {
  if (idx < 0 || idx >= n_vars) return 0;
  return var_data[idx].shared;
}

#define MAKE_FUN(sheep)                        \
  char *web_var_##sheep(int idx) {             \
    if (idx < 0 || idx >= n_vars) return NULL; \
    return var_data[idx].sheep;                \
  }

MAKE_FUN(type)
MAKE_FUN(name)
MAKE_FUN(dims)
MAKE_FUN(value)

#undef MAKE_FUN

int web_prepare_vars(uint64_t tid) {
  if (n_vars > 0) {
    for (int i = 0; i < n_vars; i++) {
      if (var_data[i].type) free(var_data[i].type);
      if (var_data[i].name) free(var_data[i].name);
      if (var_data[i].dims) free(var_data[i].dims);
      if (var_data[i].value) free(var_data[i].value);
    }
  }
  n_vars = 0;
  int *global = NULL;
  variable_info_t **info = NULL;
  if (!env || !env->debug_info) return 0;
  int s = code_map_find(env->debug_info->scope_map, env->stored_pc);
  if (s == -1) return 0;

  for (int sid = env->debug_info->scope_map->val[s]; sid != MAP_SENTINEL;
       sid = env->debug_info->scopes[sid].parent) {
    for (int v = 0; v < env->debug_info->scopes[sid].n_vars; v++)
      if (env->debug_info->scopes[sid].vars[v].addr < env->stored_pc) {
        int found = 0;
        for (int i = 0; i < n_vars; i++)
          if (!strcmp(info[i]->name,
                      env->debug_info->scopes[sid].vars[v].name)) {
            found = 1;
            break;
          }
        if (!found) {
          n_vars++;
          global = realloc(global, n_vars * sizeof(int));
          info = realloc(info, n_vars * sizeof(variable_info_t *));
          global[n_vars - 1] =
              (env->debug_info->scopes[sid].parent == MAP_SENTINEL);
          info[n_vars - 1] = &(env->debug_info->scopes[sid].vars[v]);
        }
      }
  }

  var_data = realloc(var_data, n_vars * sizeof(var_data_t));
  thread_t *t = get_thread(tid);
  if (t == NULL) t = env->thr[0];

  for (int i = 0; i < n_vars; i++) {
    int addr = info[i]->addr;
    if (!global[i]) addr += env->frame->base;
    var_data[i].shared = (addr < t->mem_base);
    var_data[i].type = strdup(env->debug_info->types[info[i]->type].name);
    var_data[i].name = strdup(info[i]->name);
    var_data[i].dims = NULL;

    writer_t *out = writer_t_new(WRITER_STRING);
    if (info[i]->num_dim > 0) {
      out_text(out, "[");
      for (int d = 0; d < info[i]->num_dim; d++) {
        if (d > 0) out_text(out, ",");
        uint32_t size = lval(get_addr(t, (addr + 4 * (2 + d)), 4), uint32_t);
        out_text(out, "%d", size);
      }
      out_text(out, "]");
      var_data[i].dims = out->str.base;
    }
    free(out);

    out = writer_t_new(WRITER_STRING);
    input_layout_item_t it = get_layout(info[i], env);
    if (info[i]->num_dim == 0)
      print_var(out, get_addr(t, addr, 4), &it);
    else {
      int *sizes = (int *)malloc(info[i]->num_dim * sizeof(int));
      for (int j = 0; j < info[i]->num_dim; j++)
        sizes[j] = lval(get_addr(t, (addr + 4 * (2 + j)), 4), uint32_t);

      print_array(out, env, &it, info[i]->num_dim, sizes,
                  lval(get_addr(t, addr, 4), uint32_t), 0, 0);
      free(sizes);
    }
    if (it.elems) free(it.elems);
    var_data[i].value = out->str.base;
    free(out);
  }

  free(global);
  free(info);
  return n_vars;
}

int web_n_threads() { return env->a_thr; }

uint64_t web_tids() {
  if (!env || (env->state != VM_RUNNING && env->state != VM_OK)) return 0;
  tids = realloc(tids, env->a_thr * 8);
  int i = 0;
  for (int t = 0; t < env->n_thr; t++)
    if (!env->thr[t]->returned) tids[i++] = env->thr[t]->tid;
  return (uint64_t)(tids);
}

int64_t web_thread_parent(uint64_t tid) {
  thread_t *t = get_thread(tid);
  if (!t) return -1;
  if (!t->parent) return 0;
  return t->parent->tid;
}

static char *index_var = "<var>";

char *web_thread_base_name() {
  if (!env || (env->state != VM_RUNNING && env->state != VM_OK)) return NULL;
  char *name = index_var;
  if (env->debug_info) {
    variable_info_t *var = NULL;
    int s = code_map_find(env->debug_info->scope_map, env->stored_pc);
    if (s > -1) {
      for (int sid = env->debug_info->scope_map->val[s];
           !var && sid != MAP_SENTINEL;
           sid = env->debug_info->scopes[sid].parent)
        for (int i = 0; !var && i < env->debug_info->scopes[sid].n_vars; i++)
          if (env->debug_info->scopes[sid].vars[i].addr ==
              env->thr[0]->mem_base - env->frame->base)
            var = &env->debug_info->scopes[sid].vars[i];
      if (var) name = var->name;
    }
  }
  return name;
}

int32_t web_thread_base_value(uint64_t tid) {
  thread_t *t = get_thread(tid);
  if (!t) return 0;
  return lval(t->mem->data, int32_t);
}

int web_state() { return __web_state; }

char *web_name() { return src_name; }

int web_stop() {
  if (__web_state == WEB_VM_RUNNING) {
    __web_state = WEB_VM_READY;
    return 0;
  }
  return 1;
}

int web_W() { return (env) ? env->W : 0; }
int web_T() { return (env) ? env->T : 0; }

// return 1 if error occured
int web_compile(char *name, char *text) {
  delete_errors();
  if (src_name) {
    free(src_name);
    src_name = NULL;
  }
  if (ast) ast_t_delete(ast);
  if (code) writer_t_delete(code);
  __web_state = WEB_NO_CODE;

  code = writer_t_new(WRITER_STRING);

  driver_init();
  driver_set_file(name, text);
  ast = driver_parse(name);

  if (!ast->error_occured) ast->error_occured = emit_code(ast, code, 0);

  driver_destroy();
  if (!ast->error_occured) {
    __web_state = WEB_VM_READY;
    src_name = strdup(name);
  }
  return ast->error_occured;
}

// return 1 if error occured while starting
int web_start(char *input) {
  //printf("WEB START\n");
  if (__web_state == WEB_NO_CODE) return 1;
  __web_state = WEB_VM_READY;

  if (outw) writer_t_delete(outw);
  outw = writer_t_new(WRITER_STRING);
  if (env) virtual_machine_t_delete(env);
  delete_errors();

  env = virtual_machine_t_new((uint8_t *)(code->str.base), code->str.ptr);
  //printf("machine ready\n");

  if (!env) {
    __web_state = WEB_NO_CODE;
    return 1;
  }

  reader_t *r = reader_t_new(READER_STRING, input);
  int err = read_input(r, env);
  reader_t_delete(r);

  if (!err)
    __web_state = WEB_VM_RUNNING;
  else
    __web_state = WEB_VM_ERROR;

  return err;
}

int web_run(int limit, int debug) {
  if (__web_state != WEB_VM_RUNNING) return -2;
  int res = execute(env, limit, 0, debug);
  if (res == -1)
    for (int i = 0; i < env->n_out_vars; i++) {
      write_output(outw, env, i);
      out_text(outw, "\n");
    }
  if (res < -1)
    __web_state = WEB_VM_ERROR;
  else if (res == -1)
    __web_state = WEB_VM_OK;
  return res;
}

int web_current_line() {
  if (!env) return -1;
  if (env->debug_info) {
    int it = code_map_find(env->debug_info->source_items_map, env->stored_pc);
    if (it == -1) return -1;
    return env->debug_info->items[env->debug_info->source_items_map->val[it]].fl;
  }
  return env->stored_pc;
}

char *web_output() {
  if (outw) return outw->str.base;
  return NULL;
}
