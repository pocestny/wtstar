#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <code.h>
#include <errors.h>
#include <reader.h>
#include <vm.h>

void error_handler(error_t *err) {
  fprintf(stderr, "%s\n", err->msg->str.base);
}

int trace_on = 0, print_io = 0, wt_stat = 1;
char *inf;

void print_help(int argc, char **argv) {
  printf("usage: %s [-h?itx] file\n", argv[0]);
  printf("options:\n");
  printf("-h,-?     print this screen and exit\n");
  printf("-i        interactive mode (prints the expected input format) \n");
  printf("-x        don't print W/T stats \n");
  printf("-t        trace run (for debugging only)\n");

  exit(0);
}

void parse_options(int argc, char **argv) {
  for (int i = 1; i < argc; i++)
    if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "-?")) {
      print_help(argc, argv);
    } else if (!strcmp(argv[i], "-i")) {
      print_io = 1;
    } else if (!strcmp(argv[i], "-t")) {
      trace_on = 1;
    } else if (!strcmp(argv[i], "-x")) {
      wt_stat = 0;
    } else
      inf = argv[i];
}

int main(int argc, char **argv) {
  inf = NULL;
  parse_options(argc, argv);
  if (!inf) print_help(argc, argv);

  register_error_handler(&error_handler);

  FILE *f = fopen(inf, "rb");
  fseek(f, 0, SEEK_END);
  int len = ftell(f);
  fseek(f, 0, SEEK_SET);
  uint8_t *in = (uint8_t *)malloc(len);
  fread(in, 1, len, f);
  fclose(f);

  if (in[0] != SECTION_HEADER || in[1] != 1) {
    printf("invalid input file\n");
    exit(1);
  }

  virtual_machine_t *env = virtual_machine_t_new(in, len);
  free(in);

  writer_t *w = writer_t_new(WRITER_FILE);
  w->f = stdout;

  if (print_io) {
    print_types(w, env);
    out_text(w, "input:\n");
    for (int i = 0; i < env->n_in_vars; i++) {
      if (env->debug_info)
        print_var_name(w, env, env->in_vars[i].addr);
      else {
        out_text(w, "%010u (%08x) ", env->in_vars[i].addr,
                 env->in_vars[i].addr);
        if (env->in_vars[i].num_dim > 0)
          out_text(w, "(%d) ", env->in_vars[i].num_dim);
        else
          out_text(w, "    ");
        print_var_layout(w, &(env->in_vars[i]));
      }
      out_text(w, "\n");
    }
  }
  reader_t *r = reader_t_new(READER_FILE, stdin);
  if (read_input(r, env) != 0) exit(-1);
  reader_t_delete(r);
  int err = execute(env, -1, trace_on, 0);
  if (err == -1) {
    if (print_io) {
      out_text(w, "output\n");
      for (int i = 0; i < env->n_out_vars; i++) {
        if (env->debug_info)
          print_var_name(w, env, env->out_vars[i].addr);
        else {
          out_text(w, "%010u (%08x) ", env->out_vars[i].addr,
                   env->out_vars[i].addr);
          if (env->out_vars[i].num_dim > 0)
            out_text(w, "(%d) ", env->out_vars[i].num_dim);
          else
            out_text(w, "    ");
          print_var_layout(w, &(env->out_vars[i]));
        }
        out_text(w," = ");
        write_output(w, env, i);
      }
    } else {
      for (int i = 0; i < env->n_out_vars; i++) write_output(w, env, i);
    }
    if (wt_stat) out_text(w, "W/T: %d %d\n", env->W, env->T);
    return 0;
  }
  exit(err);
}
