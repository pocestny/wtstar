/**
 * @file wtdb.c
 * @brief simple ncurses-based debugger
 *  @todo write documentation
 */
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <code.h>
#include <errors.h>
#include <reader.h>
#include <vm.h>

#include <driver.h>
#include <code_generation.h>

#include <linenoise.h>

#define CYAN_BOLD "\x1b[36;1m"
#define RED_BOLD "\x1b[31;1m"
#define YELLOW_BOLD "\x1b[93;1m"
#define GREEN_BOLD "\x1b[32;1m"
#define WHITE_BOLD "\x1b[37;1m"
#define WHITE "\x1b[37;0m"
#define MAGENTA_BOLD "\x1b[35;1m"
#define TERM_RESET "\x1b[0m"

char *binary_file_name = NULL, *input_string = NULL, *input_file_name = NULL;
virtual_machine_t *env = NULL;
writer_t *outw = NULL;

uint8_t *binary_file = NULL;
int binary_length;

int input_needed = 0;
int focused_thread = -1;





int debug_source = 0;
char *source_file_name = NULL;
ast_t *ast;
include_project_t *ip;
writer_t *out;

char* strapp(char *src, char *suff) {
  src = (char *) realloc(src, 1 + strlen(src) + strlen(suff));
  return strcat(src, suff);
}

void ast_find_breakpoint(
  ast_node_t *curr_node, scope_t *curr_scope,
  ast_node_t **res_node, scope_t **res_scope
) {
  if(!curr_node) {
    for(ast_node_t *next = curr_scope->items; next != NULL; next = next->next)
      ast_find_breakpoint(next, curr_scope, res_node, res_scope);
  } else if(curr_node->node_type == AST_NODE_SCOPE) {
    scope_t *scope = curr_node->val.sc;
    for(ast_node_t *next = scope->items; next != NULL; next = next->next)
      ast_find_breakpoint(next, scope, res_node, res_scope);
  } else if(curr_node->node_type == AST_NODE_STATEMENT) {
    statement_t *stmt = curr_node->val.s;
    for(int i = 0; i < 2; ++i) {
      if(stmt->par[i])
        ast_find_breakpoint(stmt->par[i], curr_scope, res_node, res_scope);
    }
    if(stmt->variant == STMT_BREAKPOINT) {
      *res_node = curr_node;
      *res_scope = curr_scope;
      printf("found breakpoint %p %p\n", curr_node, curr_scope);
    }
  }
}

int db_add_breakpoint(virtual_machine_t *vm, ast_t *ast, char *fn) {
  ip = include_project_t_new();
  driver_init(ip);
  // TODO read from string
  // TODO add { ... } into source file

  ast_node_t *bp_node;
  scope_t *bp_scope;
  // for every item in root scope
  ast_find_breakpoint(NULL, ast->root_scope, &bp_node, &bp_scope);
  ast->current_scope = bp_scope;
  uint32_t bp_code_pos = bp_node->code_from;
  bp_scope->items = NULL;
  ast = driver_parse_from(ast, ip, fn);
  list_append(ast_node_t, &bp_scope->items, bp_node);

  out = writer_t_new(WRITER_STRING);
  int resp = emit_code_scope_section(ast, bp_scope->items->val.sc, out);
  
  if (resp) {
    error_t *err = error_t_new();
    append_error_msg(err, "there were errors");
    emit_error(err);
    return 1;
  }

  uint32_t code_size = out->str.ptr; // remove last instruction (ENDVM)
  uint8_t *code = (uint8_t*)out->str.base;
  if (!( // this is only a heuristic
    code[code_size - 1] == ENDVM &&
    code[code_size - 2] == MEM_FREE &&
    code[code_size - 3] != MEM_FREE
  )) {
    printf("%sno final expression%s\n", RED_BOLD, TERM_RESET);
    return 1;
  }

  printf("breakpoint code_size %d\n", code_size);
  add_breakpoint(vm, bp_code_pos, code, code_size);

  writer_t_delete(out);
  return 0;
}




void describe(virtual_machine_t *cenv) {
  printf("loaded file:  %s%s%s\n", CYAN_BOLD, binary_file_name, TERM_RESET);
  int cenv_need_free = 0;
  if (cenv == NULL) {
    cenv = virtual_machine_t_new(binary_file, binary_length);
    cenv_need_free = 1;
  }
  if (!cenv) {
    printf("%scorrupted file%s\n", RED_BOLD, TERM_RESET);
    input_needed = 0;
    return;
  }
  if (cenv->debug_info) {
    printf("source files: %s", CYAN_BOLD);
    for (int i = 0; i < cenv->debug_info->n_files; i++)
      printf("%s ", cenv->debug_info->files[i]);
    printf("%s\n", TERM_RESET);
  }
  printf("memory mode:  %s\n", mode_name(cenv->mem_mode));
  printf("%sinput variables%s\n", CYAN_BOLD, TERM_RESET);
  print_io_vars(outw, cenv, cenv->n_in_vars, cenv->in_vars);
  printf("%soutput variables%s\n", CYAN_BOLD, TERM_RESET);
  print_io_vars(outw, cenv, cenv->n_out_vars, cenv->out_vars);
  printf("--\n");
  printf("CODE\n");
  print_code(outw, cenv->code, cenv->code_size);
  printf("HEADER\n");
  dump_header(outw, cenv);
  printf("TYPES\n");
  print_types(outw, cenv);
  printf("DEBUG INFO\n");
  dump_debug_info(outw, cenv);
  input_needed = 0;
  if (cenv->n_in_vars > 0) input_needed = 1;
  if (cenv_need_free)
    virtual_machine_t_delete(cenv);
}

void show_threads() {
  static char *index_var = "[index]";

  if (!env || (env->state != VM_RUNNING && env->state != VM_OK)) {
    printf("%sno program running%s\n", YELLOW_BOLD, TERM_RESET);
    return;
  }
  for (int t = 0; t < env->n_thr; t++)
    if (!env->thr[t]->returned) {
      printf("id %3lu ", env->thr[t]->tid);

      if (env->n_thr > 1) {
        // find index var
        char *name = index_var;
        if (env->debug_info) {
          variable_info_t *var = NULL;
          int s = code_map_find(env->debug_info->scope_map, env->stored_pc);
          if (s > -1) {
            for (int sid = env->debug_info->scope_map->val[s];
                 !var && sid != MAP_SENTINEL;
                 sid = env->debug_info->scopes[sid].parent)
              for (int i = 0; !var && i < env->debug_info->scopes[sid].n_vars;
                   i++)
                if (env->thr[t]->mem_base >= env->frame->base &&
                    env->debug_info->scopes[sid].vars[i].addr ==
                        env->thr[t]->mem_base - env->frame->base)
                  var = &env->debug_info->scopes[sid].vars[i];
            if (var) name = var->name;
          }
        }
        printf("parent %3lu : %s=%d ",
               (env->thr[t]->parent) ? env->thr[t]->parent->tid : -1, name,
               lval(env->thr[t]->mem->data, int32_t));
      }
      if (env->thr[t]->bp_hit) printf("%s[hit]%s", GREEN_BOLD, TERM_RESET);
      printf("\n");
    }
}

void load_file() {
  input_needed = 0;
  if (binary_file) free(binary_file);
  binary_file = NULL;
  FILE *f = fopen(binary_file_name, "rb");
  if (!f) {
    printf("%scannot open %s%s\n", RED_BOLD, binary_file_name, TERM_RESET);
    printf("no file loaded\n");
    return;
  }
  fseek(f, 0, SEEK_END);
  binary_length = ftell(f);
  fseek(f, 0, SEEK_SET);
  binary_file = (uint8_t *)malloc(binary_length);
  fread(binary_file, 1, binary_length, f);
  fclose(f);

  if (binary_file[0] != SECTION_HEADER || binary_file[1] != 1) {
    printf("%s%s is not a valid binary file%s\n", RED_BOLD, binary_file_name,
           TERM_RESET);
    printf("no file loaded\n");
    free(binary_file);
    binary_file = NULL;
    return;
  }
  if (env) {
    printf("%sresetting active program%s\n", YELLOW_BOLD, TERM_RESET);
    virtual_machine_t_delete(env);
    env = NULL;
  }
  describe(NULL);
}

void print_input() {
  if (input_string == NULL && input_file_name == NULL) {
    printf("no input\n");
    return;
  }
  if (input_string) {
    printf("input is %s<string>%s\n%s\n", CYAN_BOLD, TERM_RESET, input_string);
    return;
  }
}

void set_input_string(char *input) {
  if (input_string) free(input_string);
  input_string = strdup(input);
  if (env) {
    printf("%sstopping active program%s\n", YELLOW_BOLD, TERM_RESET);
    virtual_machine_t_delete(env);
    env = NULL;
  }
}

void cont() {
  if (!env || !(env->state == VM_READY || env->state == VM_RUNNING)) {
    printf("%sno program running%s\n", RED_BOLD, TERM_RESET);
    return;
  }

  int err = execute(env, -1, 0, 1);
  if (err == -1) {
    printf("%sprogram finished%s\n", CYAN_BOLD, TERM_RESET);
    for (int i = 0; i < env->n_out_vars; i++) {
      write_output(outw, env, i);
    }
    out_text(outw, "%swork: %d\ntime: %d%s\n", CYAN_BOLD, env->W, env->T,
             TERM_RESET);
  } else if (err > 0) {
    int hits = 0;
    for (int t = 0; t < env->n_thr; t++)
      if (!env->thr[t]->returned && env->thr[t]->bp_hit) hits++;
    printf("%sbreakpoint @%d hit by %d thread(s)%s\n", GREEN_BOLD, err, hits,
           TERM_RESET);
    if (env->debug_info) {
      int l = code_map_find(env->debug_info->source_items_map, env->stored_pc);
      if (l > -1) {
        int loc = env->debug_info->source_items_map->val[l];
        if (loc >= 0 && loc < env->debug_info->n_items) {
          printf("%s%s: %d %s\n", YELLOW_BOLD,
                 env->debug_info->files[env->debug_info->items[loc].fileid],
                 env->debug_info->items[loc].fl, TERM_RESET);
        }
      }
    }
  }
}

void run() {
  if (!binary_file) {
    printf("%sno binary file loaded%s\n", RED_BOLD, TERM_RESET);
    return;
  }
  if (input_needed && input_string == NULL && input_file_name == NULL) {
    printf("%sno input%s\n", RED_BOLD, TERM_RESET);
    return;
  }

  if (env) {
    if (env->state != VM_READY)
      printf("%srestarting program%s\n", YELLOW_BOLD, TERM_RESET);
    virtual_machine_t_delete(env);
    env = NULL;
  }

  env = virtual_machine_t_new(binary_file, binary_length);
  if (input_needed) {
    if (input_string) {
      reader_t *in = reader_t_new(READER_STRING, input_string);
      if (read_input(in, env) != 0) {
        virtual_machine_t_delete(env);
        env = NULL;
      }
      reader_t_delete(in);
      if (!env) return;
    } else {
      printf("%sno input given%s\n", RED_BOLD, TERM_RESET);
      virtual_machine_t_delete(env);
      env = NULL;
      return;
    }
  }

      describe(env);
      db_add_breakpoint(env, ast, "breakpoint.wt"); // TODO
      describe(env);

  cont();
}

int variable_visible(int sid, int v) {
  int visible = 0;
  if (env->debug_info->scopes[sid].parent == MAP_SENTINEL) {
    // global variable
    if (env->debug_info->scopes[sid].vars[v].from_code < env->last_global_pc)
      visible = 1;
  } else {
    if (env->debug_info->scopes[sid].vars[v].from_code < env->stored_pc)
      visible = 1;
  }

  return visible;
}

void variable_list() {
  int n_vars = 0;
  int *global = NULL;
  variable_info_t **info = NULL;

  if (env && env->debug_info) {
    int s = code_map_find(env->debug_info->scope_map, env->stored_pc);
    if (s == -1) return;

    for (int sid = env->debug_info->scope_map->val[s]; sid != MAP_SENTINEL;
         sid = env->debug_info->scopes[sid].parent) {
      for (int v = 0; v < env->debug_info->scopes[sid].n_vars; v++) {
        if (variable_visible(sid, v)) {
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
    }
    for (int i = 0; i < n_vars; i++) {
      int addr = info[i]->addr;
      if (!global[i]) addr += env->frame->base;
      if (addr < env->thr[0]->mem_base)
        printf(WHITE_BOLD);
      else
        printf(WHITE);
      printf("%s %s", env->debug_info->types[info[i]->type].name,
             info[i]->name);
      if (info[i]->num_dim > 0) {
        printf("[");
        for (int d = 0; d < info[i]->num_dim; d++) {
          if (d > 0) printf(",");
          printf("_");
        }
        printf("]");
      }
      printf("%s\n", TERM_RESET);
    }
    free(global);
    free(info);
  }
}

void print_variable_in_thread(char *name) {
  if(name == NULL) {
    printf("Empty variable name\n");
    return;
  }
  if (!env || !env->debug_info) return;
  thread_t *t = get_thread(focused_thread);

  if (focused_thread > -1) printf("focused thread %d\n", focused_thread);
  if (t == NULL) {
    printf("%sfocused thread not active, assuming id %lu%s\n", YELLOW_BOLD,
           env->thr[0]->tid, TERM_RESET);
    t = env->thr[0];
  }

  int s = code_map_find(env->debug_info->scope_map, env->stored_pc);
  if (s == -1) return;

  variable_info_t *var = NULL;
  int global = 0;

  for (int sid = env->debug_info->scope_map->val[s];
       !var && sid != MAP_SENTINEL; sid = env->debug_info->scopes[sid].parent) {
    for (int v = 0; !var && v < env->debug_info->scopes[sid].n_vars; v++) {
      if (variable_visible(sid, v) &&
          !strcmp(name, env->debug_info->scopes[sid].vars[v].name)) {
        var = &env->debug_info->scopes[sid].vars[v];
        if (env->debug_info->scopes[sid].parent == MAP_SENTINEL) global = 1;
        break;
      }
    }
  }

  if(var == NULL) {
    printf("Variable not found\n");
    return;
  }

  int addr = var->addr;
  if (!global) addr += env->frame->base;
  if (addr < t->mem_base)
    printf(WHITE_BOLD);
  else
    printf(WHITE);
  printf("%s %s", env->debug_info->types[var->type].name, var->name);
  if (var->num_dim > 0) {
    printf("[");
    for (int d = 0; d < var->num_dim; d++) {
      if (d > 0) printf(",");
      uint32_t size = lval(get_addr(t, (addr + 4 * (2 + d)), 4), uint32_t);
      printf("%d", size);
    }
    printf("]");
  }
  printf(" = ");
  input_layout_item_t it = get_layout(var, env);
  if (var->num_dim == 0)
    print_var(outw, get_addr(t, addr, 4), &it);
  else {
    int *sizes = (int *)malloc(var->num_dim * sizeof(int));
    for (int i = 0; i < var->num_dim; i++)
      sizes[i] = lval(get_addr(t, (addr + 4 * (2 + i)), 4), uint32_t);
    print_array(outw, env, &it, var->num_dim, sizes,
                lval(get_addr(t, addr, 4), uint32_t), 0, 0);
    free(sizes);
  }
  printf("%s\n", TERM_RESET);
  if (it.elems) free(it.elems);
}

void error_handler(error_t *err) {
  fprintf(stderr, "%s%s%s\n", RED_BOLD,err->msg->str.base,TERM_RESET);
}

void print_help(int argc, char **argv) {
  printf("usage: %s file\n", argv[0]);
  printf("options:\n");
  printf("-h,-?         print this screen and exit\n");
  exit(0);
}

void parse_options(int argc, char **argv) {
  for (int i = 1; i < argc; i++)
    if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "-?")) {
      print_help(argc, argv);
    } else
      binary_file_name = argv[i];
}

const char *const keywords[] = {"quit",   "set",      "input",    "output",
                                "string", "file",     "reload",   "print",
                                "run",    "continue", "help",     "describe",
                                "list",   "thread",   "variable", "????"};

typedef enum {
  T_QUIT = 0,
  T_SET,
  T_INPUT,
  T_OUTPUT,
  T_STRING,
  T_FILE,
  T_RELOAD,
  T_PRINT,
  T_RUN,
  T_CONTINUE,
  T_HELP,
  T_DESCRIBE,
  T_LIST,
  T_THREAD,
  T_VARIABLE,
  T_NONE
} tokens_t;

#define MAX_TOKENS 4

// returns position of first non-token
int parse(const char *in_str, tokens_t *result) {
  int pos = -1;
  char *str = strdup(in_str), *s = strtok(str, " \t");
  for (int i = 0; i < MAX_TOKENS; i++) result[i] = T_NONE;
  for (int i = 0; s && i < MAX_TOKENS; i++, s = strtok(NULL, " \t")) {
    int found = 0;
    for (int j = 0; keywords[j][0] != '?'; j++)
      if (!strcmp(keywords[j], s)) {
        result[i] = j;
        found = 1;
        break;
      }
    if (!found) {
      pos = s - str;
      break;
    }
  }
  free(str);
  return pos;
}

const char *const commands[] = {"quit",           "set input string",
                                "set input file", "reload",
                                "input",          "run",
                                "continue",       "help",
                                "describe",       "thread list",
                                "variable list",  "set thread",
                                "print",          "????"};

void completion(const char *buf, linenoiseCompletions *lc) {
  static tokens_t tokens[MAX_TOKENS];
  int pos = parse(buf, tokens);
  char *prefix = strdup("");

  for (int i = 0; i < MAX_TOKENS && tokens[i] != T_NONE; i++) {
    prefix = realloc(prefix, strlen(prefix) + strlen(keywords[tokens[i]]) + 2);
    strcat(prefix, keywords[tokens[i]]);
    strcat(prefix, " ");
  }

  char *cmd;
  char *rest;
  if (pos >= 0) {
    rest = (char *)buf + pos;
    cmd = malloc(strlen(prefix) + strlen(rest) + 2);
    cmd[0] = 0;
    strcat(cmd, prefix);
    strcat(cmd, rest);
  } else {
    cmd = strdup(prefix);
    rest = strdup("");
  }

  for (int i = 0; keywords[i][0] != '?'; i++)
    if (!strncmp(rest, keywords[i], strlen(rest))) {
      int found = 0;
      char *tmp = malloc(strlen(prefix) + strlen(keywords[i]) + 4);
      tmp[0] = 0;
      strcat(tmp, prefix);
      strcat(tmp, keywords[i]);
      for (int j = 0; commands[j][0] != '?'; j++)
        if (!strncmp(tmp, commands[j], strlen(tmp))) {
          found = 1;
          break;
        }
      if (found) linenoiseAddCompletion(lc, tmp);
      free(tmp);
    }

  if (pos < 0) free(rest);
  free(prefix);
  free(cmd);
}

void parse_args(int argc, char **argv) {
  parse_options(argc, argv);
  if (!binary_file_name) print_help(argc, argv);
  outw = writer_t_new(WRITER_FILE);
  outw->f = stdout;
  {
    int sl = strlen(binary_file_name);
    if(sl >= 3 && !strcmp(binary_file_name + sl - 3, ".wt")) {
      debug_source = 1;
      source_file_name = binary_file_name;
      binary_file_name = strdup(source_file_name);
      binary_file_name = strapp(binary_file_name, ".out");

      ip = include_project_t_new();
      driver_init(ip);
      ast = driver_parse(ip, source_file_name);

      out = writer_t_new(WRITER_FILE);
      out->f = fopen(binary_file_name, "wb");
      int resp = emit_code(ast, out, 0);
      writer_t_delete(out);
      
      if (resp) {
        error_t *err = error_t_new();
        append_error_msg(err, "there were errors");
        emit_error(err);
        exit(1);
      }
    }
  }
}

int parse_cmd(char **response) {
  printf(WHITE_BOLD);
  fflush(stdout);
  char *result_raw = linenoise(">> ");
  printf(TERM_RESET);
  fflush(stdout);
  if(!result_raw) {
    printf("EOF\n");
    return 0; // exit
  }
  linenoiseHistoryAdd(result_raw);

  tokens_t tokens[MAX_TOKENS];
  int pos = parse(result_raw, tokens);
  char *result = malloc(strlen(result_raw) + 10);
  result[0] = 0;
  for (int i = 0; i < MAX_TOKENS && tokens[i] != T_NONE; i++) {
    strcat(result, keywords[tokens[i]]);
    strcat(result, " ");
  }
  if (pos != -1)
    strcat(result, result_raw + pos);

  int cmd = -1;
  for (int i = 0; commands[i][0] != '?'; i++)
    if (!strncmp(commands[i], result, strlen(commands[i]))) {
      cmd = i;
      break;
    }

  free(result_raw);

  *response = result;
  return cmd;
}

int main(int argc, char **argv) {
  parse_args(argc, argv);
  load_file();

  register_error_handler(&error_handler);
  linenoiseSetCompletionCallback(completion);

  printf("type %shelp%s for the list of commands\n", WHITE_BOLD, TERM_RESET);

  int running = 1;
  while (running) {
    char *response;
    int cmd = parse_cmd(&response);
    switch (cmd) {
      case 0:
        running = 0;
        break;
      case 1:
        // set input string
        set_input_string(response + 17);
        break;
      case 2:
        printf("set input file\n");
        break;
      case 3:
        // reload
        load_file();
        break;
      case 4:
        // input
        print_input();
        break;
      case 5:
        // run
        run();
        break;
      case 6:
        // continue
        cont();
        break;
      case 7:
        // help
        printf("available commands: ");
        printf(WHITE_BOLD);
        for (int i = 0; commands[i][0] != '?'; i++)
          printf("%s%s", (i == 0) ? "" : ", ", commands[i]);
        printf("%s\n", TERM_RESET);
        break;
      case 8:
        describe(env);
        break;
      case 9:
        show_threads();
        break;
      case 10:
        variable_list();
        break;
      case 11:  // set thread
        sscanf(response + 11, "%d", &focused_thread);
        break;
      case 12:
        print_variable_in_thread(strtok(response + 5, " \t"));
        break;
      default:
        printf("unknown command\n");
        break;
    }
    free(response);
  }
  
  virtual_machine_t_delete(env);
  driver_destroy(ip);
  ast_t_delete(ast);
}
