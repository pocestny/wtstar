#include "driver.h"
#include "writer.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "code_generation.h"

// extern int yydebug;
#include "ast_debug_print.h"

char *outf, *inf, *astf;
int ast_debug = 0, outf_spec = 0;

void print_help(int argc, char **argv) {
  printf("usage: %s [-h][-?][-D][-o file] file\n", argv[0]);
  printf("options:\n");
  printf("-h,-?         print this screen and exit\n");
  printf("-o file       write output to file\n");
  printf("-D file       print intermediate AST to file (- = stderr)\n");
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
      if (++i < argc)
        astf = argv[i];
      else {
        print_help(argc, argv);
        exit(1);
      }
    } else if (!strcmp(argv[i], "-D-")) {
      ast_debug = 1;
      astf = argv[i] + 2;
    } else
      inf = argv[i];
}

int main(int argc, char **argv) {
  outf = "a.out";
  inf = NULL;
  astf = NULL;
  parse_options(argc, argv);
  if (!inf) print_help(argc, argv);

  // yydebug=1;
  driver_init();
  ast_t *r = driver_parse(inf);

  writer_t *write_out;

  if (!ast_debug) {
    write_out = writer_t_new(WRITER_FILE);
    write_out->f = fopen(outf, "wb");
  }

  writer_t *write_log = writer_t_new(WRITER_FILE);
  write_log->f = stderr;

  writer_t *write_ast;
  if (ast_debug) {
    write_ast = writer_t_new(WRITER_FILE);
    if (!strcmp(astf, "-"))
      write_ast->f = stderr;
    else
      write_ast->f = fopen(astf, "wt");
  }

  if (r->error_occured)
    out_text(write_log, "there were errors\n");
  else {
    if (ast_debug)
      ast_debug_print(r, write_ast);
    else
      emit_code(r, write_out, write_log);
  }

  driver_destroy();
  ast_t_delete(r);

  if (!ast_debug) writer_t_delete(write_out);
  writer_t_delete(write_log);
  if (ast_debug) writer_t_delete(write_ast);
}
