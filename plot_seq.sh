#!/bin/sh

./wtc -o sum.out sum.wt

rm -rf sum.data

for n in `seq 1 20`; do
  m=`echo "2^$n"|bc`
  echo $m `seq 1 $m` | ./wtr sum.out | grep W | sed -e "s/^[^ ]* /$m /g" >> sum.data
done

echo "plot 'sum.data' using 1:3 with lines title \"work\"; pause mouse " | gnuplot
echo "plot 'sum.data' using 1:2 with lines title \"time\"; pause mouse" | gnuplot

rm sum.data
