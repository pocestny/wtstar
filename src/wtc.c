#include "driver.h"
#include "writer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

// extern int yydebug;

char * outf, *inf;

void print_help(int argc,char**argv) {
      printf("usage: %s [-h][-?][-o file] file\n",argv[0]);
      exit(0);
}

void parse_options(int argc, char **argv) {
  for (int i=1;i<argc;i++) 
    if (!strcmp(argv[i],"-h")||!strcmp(argv[i],"-?")) {
      print_help(argc,argv);
    } else if ((!strcmp(argv[i],"-o"))) {
      if (++i<argc) outf=argv[i];
    } else inf=argv[i];

}

int main(int argc, char **argv) {
  outf = "a.out";
  inf=NULL;
  parse_options(argc,argv);
  if (!inf) print_help(argc,argv);

  // yydebug=1;
  driver_init();
  ast_t *r = driver_parse(inf);
  driver_destroy();

  writer_t *out = writer_t_new(WRITER_FILE);
  out->f=fopen(outf,"wb");
  writer_t *log = writer_t_new(WRITER_FILE);
  log->f=stderr;
  if (r->error_occured)
    out_text(log,"there were errors\n");
  else
    emit_code(r,out,log);
  ast_t_delete(r);

  writer_t_delete(out);
  writer_t_delete(log);

  /*for(int i=0;i<out->str.ptr;i++)
    printf("%02x ",out->str.base[i]);
  printf("\n");
  */
}
