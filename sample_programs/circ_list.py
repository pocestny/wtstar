#!/usr/bin/env python3
import random as rand
import sys

if __name__ == '__main__':
    n=int(sys.argv[1])
    perm = rand.sample(range(n),k=n)
    cl=[0]*n
    for i in range(n):
            cl[perm[i]]=perm[(i+1)%n]
    print('[',*cl,']',sep=' ')
