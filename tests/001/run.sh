#!/bin/bash

make 

valgrind --log-fd=9 9>valgrind.log --leak-check=full --show-leak-kinds=all ./test_hash

cat valgrind.log
rm -rf test_hash valgrind.log
