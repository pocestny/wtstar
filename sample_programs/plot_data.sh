#!/bin/sh

echo "\
  set term png;\
  set output 'work.png'; \
  plot \"$1\" using 1:2 with linespoints title 'work' \
" | gnuplot

echo "\
  set term png;\
  set output 'time.png'; \
  plot \"$1\" using 1:3 with linespoints title 'time' \
" | gnuplot

convert work.png time.png +append `basename $1 .perf`.png
rm work.png time.png

