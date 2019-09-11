#include <hash.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

void deleter(void *x) {
  free(x);
}

int main() {
  hash_table_t *t = hash_table_t_new(0,deleter);

  hash_put(t,42,strdup("the original answer"));
  printf("%s\n",hash_get(t,42));
  hash_put(t,12345,strdup("quaaak"));
  for (int i=0;i<50;i++) {
    char buf[200];
    sprintf(buf,"the answer #%d",i);
    hash_put(t,i,strdup(buf));
  }
  printf("%s\n",hash_get(t,42));
  printf("%s\n",hash_get(t,12345));

  hash_table_t_delete(t);
}
