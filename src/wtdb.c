#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <code.h>
#include <errors.h>
#include <reader.h>
#include <vm.h>

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

void include_layout_type(input_layout_item_t *it,  virtual_machine_t *env, int type) {
  type_info_t *t = &(env->debug_info->types[type]);

  if (t->n_members==0) {
    it->n_elems++;
    it->elems=realloc(it->elems,it->n_elems);
    if (!strcmp(t->name,"int")) it->elems[it->n_elems-1]=TYPE_INT;
    else if (!strcmp(t->name,"float")) it->elems[it->n_elems-1]=TYPE_FLOAT;
    else if (!strcmp(t->name,"char")) it->elems[it->n_elems-1]=TYPE_CHAR;
    else exit(123);
  } else
    for (int m=0;m<t->n_members;m++)
      include_layout_type(it,env,t->member_types[m]);
}

input_layout_item_t get_layout(variable_info_t *var, virtual_machine_t *env) {
  input_layout_item_t r;
  r.addr=var->addr;
  r.num_dim = var->num_dim;
  r.n_elems=0;
  r.elems=NULL;
  include_layout_type(&r,env,var->type);
  return r;
}


void describe() {
  printf("loaded file:  %s%s%s\n", CYAN_BOLD, binary_file_name, TERM_RESET);
  virtual_machine_t *tmp = virtual_machine_t_new(binary_file, binary_length);
  if (!tmp) {
    printf("%scorrupted file%s\n", RED_BOLD, TERM_RESET);
    input_needed = 0;
    return;
  }
  if (tmp->debug_info) {
    printf("source files: %s", CYAN_BOLD);
    for (int i = 0; i < tmp->debug_info->n_files; i++)
      printf("%s ", tmp->debug_info->files[i]);
    printf("%s\n", TERM_RESET);
  }
  printf("memory mode:  %s\n", mode_name(tmp->mem_mode));
  printf("%sinput variables%s\n", CYAN_BOLD, TERM_RESET);
  print_io_vars(outw, tmp, tmp->n_in_vars, tmp->in_vars);
  printf("%soutput variables%s\n", CYAN_BOLD, TERM_RESET);
  print_io_vars(outw, tmp, tmp->n_out_vars, tmp->out_vars);
  input_needed = 0;
  if (tmp->n_in_vars > 0) input_needed = 1;
  virtual_machine_t_delete(tmp);
}

void show_threads() {
  static char *index_var = "<var>";

  if (!env || (env->state != VM_RUNNING && env->state != VM_OK)) {
    printf("%sno program running%s\n", YELLOW_BOLD, TERM_RESET);
    return;
  }
  for (int t = 0; t < env->n_thr; t++)
    if (!env->thr[t]->returned) {
      printf("id %3d ", env->thr[t]->tid);

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
                if (env->debug_info->scopes[sid].vars[i].addr ==
                    env->thr[t]->mem_base - env->frame->base)
                  var = &env->debug_info->scopes[sid].vars[i];
            if (var) name = var->name;
          }
        }
        printf("parent %3d : %s=%d ",
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
  describe();
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

void run();

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
        printf("%swrong input%s\n", RED_BOLD, TERM_RESET);
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

  cont();
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
  if (!env || !env->debug_info) return;
  int t = -1;
  for (int i = 0; i < env->n_thr; i++)
    if (!env->thr[i]->returned)
      if (env->thr[i]->tid == focused_thread) {
        t = i;
        break;
      }
  if (focused_thread > -1) printf("focused thread %d\n", focused_thread);
  if (t == -1) {
    printf("%sfocused thread not active, assuming id %d%s\n", YELLOW_BOLD,
           env->thr[0]->tid,TERM_RESET);
    t = 0;
  }

  int s = code_map_find(env->debug_info->scope_map, env->stored_pc);
  if (s == -1) return;

  variable_info_t *var = NULL;
  int global = 0;

  for (int sid = env->debug_info->scope_map->val[s];
       !var && sid != MAP_SENTINEL; sid = env->debug_info->scopes[sid].parent)
    for (int v = 0; !var && v < env->debug_info->scopes[sid].n_vars; v++)
      if (!strcmp(name, env->debug_info->scopes[sid].vars[v].name)) {
        var = &env->debug_info->scopes[sid].vars[v];
        if (env->debug_info->scopes[sid].parent == MAP_SENTINEL) global = 1;
        break;
      }

  int addr = var->addr;
  if (!global) addr += env->frame->base;
  if (addr < env->thr[t]->mem_base)
    printf(WHITE_BOLD);
  else
    printf(WHITE);
  printf("%s %s", env->debug_info->types[var->type].name, var->name);
  if (var->num_dim > 0) {
    printf("[");
    for (int d = 0; d < var->num_dim; d++) {
      if (d > 0) printf(",");
      uint32_t size = lval(get_addr(env->thr[t],(addr+4*(2+d)),4),uint32_t);
      printf("%d",size);
    }
    printf("]");
  }
  printf(" = ");
  input_layout_item_t it = get_layout(var, env);
  if (var->num_dim==0) 
   print_var(outw,get_addr(env->thr[t],addr,4),&it);
  else {
    int *sizes = (int *)malloc(var->num_dim*sizeof(int));
    for(int i=0;i<var->num_dim;i++) 
      sizes[i]=lval(get_addr(env->thr[t],(addr+4*(2+i)),4),uint32_t);
   print_array(outw,env,&it,var->num_dim, sizes, 
       lval(get_addr(env->thr[t],addr,4),uint32_t),0,0);
   free(sizes);
  }
  printf("%s\n", TERM_RESET);
  if (it.elems) free(it.elems);
}

void error_handler(error_t *err) {
  fprintf(stderr, "%s\n", err->msg->str.base);
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

#define MAX_TOKENS 3

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

int main(int argc, char **argv) {
  parse_options(argc, argv);
  if (!binary_file_name) print_help(argc, argv);
  outw = writer_t_new(WRITER_FILE);
  outw->f = stdout;
  load_file();

  register_error_handler(&error_handler);
  linenoiseSetCompletionCallback(completion);

  printf("type %shelp%s for the list of commands\n", WHITE_BOLD, TERM_RESET);
  tokens_t tokens[MAX_TOKENS];

  while (1) {
    printf(WHITE_BOLD);
    fflush(stdout);
    char *result_raw = linenoise(">> ");
    printf(TERM_RESET);
    fflush(stdout);
    linenoiseHistoryAdd(result_raw);

    int pos = parse(result_raw, tokens);
    char *result = malloc(strlen(result_raw) + 10);
    result[0] = 0;
    for (int i = 0; i < MAX_TOKENS && tokens[i] != T_NONE; i++) {
      strcat(result, keywords[tokens[i]]);
      strcat(result, " ");
    }
    strcat(result, result_raw + pos);
    int cmd = -1;
    for (int i = 0; commands[i][0] != '?'; i++)
      if (!strncmp(commands[i], result, strlen(commands[i]))) {
        cmd = i;
        break;
      }
    switch (cmd) {
      case 0:
        exit(0);
        break;
      case 1:
        // set input string
        set_input_string(result + 17);
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
        describe();
        break;
      case 9:
        show_threads();
        break;
      case 10:
        variable_list();
        break;
      case 11:  // set thread
        sscanf(result + 11, "%d", &focused_thread);
        break;
      case 12:
        print_variable_in_thread(strtok(result + 5," \t"));
        break;
      default:
        printf("unknown command\n");
        break;
    }
    free(result);
    free(result_raw);
  }
}
