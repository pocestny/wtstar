#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <code_generation.h>
#include <driver.h>
#include <writer.h>
#include <errors.h>

//extern int yydebug;
#include <ast_debug_print.h>

char *outf, *inf;
int ast_debug = 0, outf_spec = 0;

void print_help(int argc, char **argv) {
  printf("usage: %s [-h][-?][-D][-o file] file\n", argv[0]);
  printf("options:\n");
  printf("-h,-?         print this screen and exit\n");
  printf("-o file       write output to file \n");
  printf("-D            print intermediate AST instead of code \n");
  exit(0);
}

void parse_options(int argc, char **argv) {
  for (int i = 1; i < argc; i++)
    if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "-?")) {
      print_help(argc, argv);
    } else if ((!strcmp(argv[i], "-o"))) {
      if (++i < argc)
        outf = argv[i];
      else {
        print_help(argc, argv);
        exit(1);
      }
      outf_spec = 1;
    } else if (!strcmp(argv[i], "-D")) {
      ast_debug = 1;
    } else
      inf = argv[i];
}


void error_handler(error_t *err) {
  fprintf(stderr,"%s\n",err->msg->str.base);
}

int main(int argc, char **argv) {
  outf = NULL;
  inf = NULL;
  parse_options(argc, argv);
  if (!inf) print_help(argc, argv);

  register_error_handler(&error_handler);
//   yydebug=1;

  driver_init();
  ast_t *r = driver_parse(inf);
  driver_destroy();

  writer_t *out;
  out = writer_t_new(WRITER_FILE);

  if (ast_debug) {
    if (!outf || !strcmp(outf, "-"))
      out->f = stdout;
    else 
      out->f = fopen(outf, "wt");
  } else {
    if (outf && !strcmp(outf, "-"))
      out->f = stdout;
    else {
      if (!outf) outf="a.out";
      out->f = fopen(outf, "wb");
    }
  }

  int err=0;
  if (r->error_occured) {
    err=1;
    error_t *err = error_t_new();
    append_error_msg(err,"there were errors");
    emit_error(err);
  } else if (ast_debug)
      ast_debug_print(r, out);
  else
      emit_code(r, out);

  ast_t_delete(r);

  writer_t_delete(out);
  return err;
}
