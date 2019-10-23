#!/bin/bash
cd snippets;
rm -rf ../snippets.cc
for f in *; do
  xxd -i $f >> ../snippets.cc
  echo $f'['$f'_len-1]=0;' >> ../snippets.cc
  echo 'snippets["'`basename $f`'"]=(char*)('$f');' >> ../snippets.cc 
done
