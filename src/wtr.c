#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <code.h>
#include <vm.h>
#include <reader.h>
#include <runtime.h>
#include <errors.h>

void error_handler(error_t *err) {
  fprintf(stderr,"%s\n",err->msg->str.base);
  exit(1);
}


int EXEC_DEBUG = 0;


int print_file = 0, print_io = 0, dump_heap=0;
char *inf;

void print_help(int argc, char **argv) {
  printf("usage: %s [-h][-?][-D] file\n", argv[0]);
  printf("options:\n");
  printf("-h,-?         print this screen and exit\n");
  printf("-i            dump io structure \n");
  printf("-D            dump file and exit\n");
  printf("-m            dump heap after finish\n");
  printf("-t            trace run\n");

  exit(0);
}

void parse_options(int argc, char **argv) {
  for (int i = 1; i < argc; i++)
    if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "-?")) {
      print_help(argc, argv);
    } else if (!strcmp(argv[i], "-D")) {
      print_file = 1;
    } else if (!strcmp(argv[i], "-i")) {
      print_io = 1;
    } else if (!strcmp(argv[i], "-m")) {
      dump_heap = 1;
    } else if (!strcmp(argv[i], "-t")) {
      EXEC_DEBUG = 1;
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

  runtime_t *env = runtime_t_new(in, len);
  free(in);

  writer_t *w = writer_t_new(WRITER_FILE);
  w->f = stdout;

  if (print_file) {
    out_text(w,"input variables:\n");
    print_io_vars(w,env->n_in_vars, env->in_vars);
    out_text(w,"\n");
    out_text(w,"output variables:\n");
    print_io_vars(w,env->n_out_vars, env->out_vars);
    out_text(w,"\n");
    out_text(w,"function addresses:\n");
    for (uint32_t i = 0; i < env->fcnt; i++)
      out_text(w,"%3u %05u\n", i, env->fnmap[i]);
    out_text(w,"\n");
    out_text(w,"code:\n");
    print_code(w,env->code, env->code_size);
  } else if (print_io) {
    out_text(w,"input variables:\n");
    print_io_vars(w,env->n_in_vars, env->in_vars);
    out_text(w,"\n");
    out_text(w,"output variables:\n");
    print_io_vars(w,env->n_out_vars, env->out_vars);
  } else {
    reader_t *r = reader_t_new(READER_FILE,stdin);
    read_input(r,env);
    reader_t_delete(r);
    execute(env, -1);
    write_output(w,env);
    out_text(w,"W/T: %d %d\n", env->W, env->T);
    if (dump_heap) dump_memory(w,env); 
  }
}
