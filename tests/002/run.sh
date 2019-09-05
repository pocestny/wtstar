#!/bin/bash

cli_path=../../_build/cli

for t in t*.wt; do
  echo $t
  ${cli_path}/wtc $t >log 2> log 
  diff log `basename $t .wt`.tst
done
rm a.out log
