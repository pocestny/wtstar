#!/bin/bash

cli_path=../../_build/cli
max_iter=10;

make random_program
n_iter=0
while [  ${n_iter} -lt ${max_iter} ]; do
  ./random_program > a.in
  ${cli_path}/wtc a.in >a.log 2>a.err
  if [ ! $? -eq 0 ]; then
    break
  fi
  let n_iter=n_iter+1 
  echo $n_iter "OK"
done
valgrind --log-fd=9 9>valgrind.log --leak-check=full --show-leak-kinds=all ${cli_path}/wtc 2>/dev/null >/dev/null a.in
cat valgrind.log
rm -rf a.in a.out a.err a.log random_program valgrind.log
