#!/usr/bin/env python3
import subprocess as su
import random as rand
import matplotlib.pyplot as plt


""" 
    baseline solution
"""
def baseline(A,y):
    for i in range(0,len(A)):
        if A[i]==y:
            return i
    return -1

def print_error(test,result,got):
    print('WRONG ANSWER: input="%s" result=%d got=%d' % (test,result,got))

n = 10000

def test_p(p,count):
    print('test %d'%p)
    w=t=0
    for test_case in range(count):
        input_list = []
        x = 0
        for i in range(n):
            x += rand.sample(range(2),1)[0]+2
            input_list.append(x)
        y = rand.sample(range(2*n),1)[0]+2
        input_string=('[ '+' '.join(['%d']*len(input_list))+' ] %d %d') % (tuple(input_list)+(y,p))
        answer = int(baseline(input_list,y))
        o = su.check_output("wtr p_ary_search.wtr".split(),input=input_string, encoding='ascii').split()
        if (int(o[0]))!=answer:
            print_error(input_string,answer,int(o[0]))
        else:
            w+=float(o[2])
            t+=float(o[3])
    return w/count,t/count


if __name__ == '__main__':
    su.Popen('wtc p_ary_search.wt -o p_ary_search.wtr'.split(), stdout=su.DEVNULL)
    W=[]
    T=[]
    P=[]
    for p in range(1,300,1):
        w,t = test_p(p,10)
        P.append(p)
        W.append(w)
        T.append(t)
    for p in range(301,1200,40):
        w,t = test_p(p,10)
        P.append(p)
        W.append(w)
        T.append(t)
    plt.subplot(211)
    plt.plot(P,T)
    plt.title('time')
    plt.xlabel('p (n=%d)'%n)
    plt.subplot(212)
    plt.plot(P,W)
    plt.title('work')
    plt.xlabel('p (n=%d)'%n)
    plt.show()


