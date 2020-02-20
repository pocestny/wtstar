#!/usr/bin/env python3

# Import the necessary packages and modules
import sys
import getopt
import math
import subprocess as su
import matplotlib as mp
#mp.use('Agg')
import matplotlib.pyplot as plt
import numpy as np
from scipy.spatial import ConvexHull
from matplotlib.patches import Polygon

#normalize output point from a hull: make them circular, and closed
def normalize(x):
    if len(x)==0:
        return []
    x = list(map(list, np.unique(np.array(x),axis=0)   ))
    cent = np.mean(np.array(x), 0)
    x.sort(key=lambda p: np.arctan2(p[1] - cent[1],
                                    p[0] - cent[0]))
    x.insert(len(x), x[0])
    x=list(map(lambda p:[float(p[0]),float(p[1])],x))
    return x
    
# baseline solution
def scipy_hull(inp):
    points=np.array(inp)
    hull = ConvexHull(points)
    pts = []
    for pt in points[hull.simplices]:
        pts.append(pt[0].tolist())
        pts.append(pt[1].tolist())
    return normalize(pts)

# format input string for wtrun
def make_input_string(data):
    return '['+' '.join(['%s']*len(data))%tuple(map(lambda x: "{ %f %f }" % tuple(x),data))+']'

def neg_pts(pts):
    return list(map(lambda x:[x[0],-x[1]],pts))

W=0
T=0
faults=0

def run_hull_half(input_string,dr):
    global W,T,faults
    pts=[]
    # print(input_string)
    try:    
        o = su.check_output("wtrun upper_hull.wtr".split(),input=input_string,encoding='ascii')
    except su.CalledProcessError as e:
        print ('EXEC FAILED')
        print(input_string)
        print (e.output)
        #print (e.returncode)
        return pts
    # print(o)
    o = o.split()
    #print(o[len(o)-2],o[len(o)-1])
    W+=int(o[len(o)-2])
    T+=int(o[len(o)-1])
    for i in range(int(o[0])):
        pts.append( [ float(o[4*i+2]),dr*float(o[4*i+3])])
    pts.sort(key=lambda p:dr*p[0])
    return pts    

def run_hull(data):
    return  normalize(run_hull_half(make_input_string(data),1) + run_hull_half(make_input_string(neg_pts(data)),-1))

# compare the baseline with wtrun solution composed of two half-hulls
def test(data):
    global faults
    ground = scipy_hull(data)
    my = run_hull(data)
    dist =np.linalg.norm(np.array(list(map(lambda p: np.array(p[0])-np.array(p[1]), zip(my,ground)))))
    if len(ground)!=len(my) or dist>0.1:
        print('TEST FAILED (%f)'%dist)
        f = open('upper_hull_fail_%d'%faults,'w')
        f.write(str(data))
        f.close()
        faults+=1
    return 


# add recursively sub-hulls
def add_sub_polys(pts):
    res=[] 
    if len(pts)<3:
        return res
    if len(pts)>2:
        m = int(len(pts)/2)
        res=add_sub_polys(pts[0:m]) + add_sub_polys(pts[m:len(pts)])
    hull = scipy_hull(pts)
    res.append(Polygon(np.array(hull),edgecolor='lightgreen',fill=False))
    return res


def plot(data):
    ground = scipy_hull(data)
    answer = run_hull_half(make_input_string(data),1)
    print(W,T)
    points=np.array(data)
    plt.plot(points[:,0], points[:,1], '.',zorder=500)
    
    for p in add_sub_polys(sorted(data)):
        plt.gca().add_patch(p)

    poly = Polygon(np.array(ground),
                   facecolor='green',alpha=0.2)
    poly.set_capstyle('round')
    plt.gca().add_patch(poly)

    if len(answer)>0:
        poly2 = Polygon(np.array(answer),edgecolor='red',fill=False,closed=False,linewidth=2,zorder=100)
        poly2.set_capstyle('round')
        plt.gca().add_patch(poly2)

        p1=Polygon(np.array(run_hull(data[0:int(len(data)/2)])),edgecolor='orange',fill=False,linewidth=1.5,zorder=50)
        plt.gca().add_patch(p1)
        p2=Polygon(np.array(run_hull(data[int(len(data)/2):len(data)])),edgecolor='orange',fill=False,linewidth=1.5,zorder=50)
        plt.gca().add_patch(p2)

    plt.show()
    return 0

help_msg = 'upper_hull.py -w <what> -n <size>\n   what = plot | run | test'


def random_data(n):
    data= list(map(list,np.random.rand(n, 2)))  
    data=list(map(lambda x:[float(1000*x[0]),float(1000*x[1])],data))
    data = list(map(list, np.unique(np.array(data),axis=0)   ))
    return data

if (__name__ == '__main__'):
    su.Popen('wtc upper_hull.wt -o upper_hull.wtr'.split(), stdout=su.DEVNULL)
    #su.Popen('wtc upper_hull_simple.wt -o upper_hull.wtr'.split(), stdout=su.DEVNULL)
    what =''
    size=80
    try:
        opts, args = getopt.getopt(sys.argv[1:],"hw:n:",["what=","size="])
    except getopt.GetoptError:
        print (help_msg)
        sys.exit(2)
    for opt, arg in opts:
        if opt == "-h":
            print (help_msg)
            sys.exit(2)
        elif opt in ("-w", "--what"):
            what = arg
        elif opt in ("-n", "--size"):
            size = int(arg)
    if what=='' or (what!='plot' and what!='run' and what!='test'):  
        print(what)
        print (help_msg)
        sys.exit(2)
   
    if what=='plot':
        #plot(data)
        plot(random_data(size))
        #plt.savefig('upper_hull.png')
    elif what=='run':
        run_hull_half(make_input_string(random_data(size)),1)
        print(W,T)
    else:
        Ws=[]
        Ts=[]
        Ns=[]
        for n in range(10,99,3):
            print(n,end=' ',flush=True)
            W=T=0
            cnt=80
            for i in range(cnt):
                test(random_data(n))
                if i%8==0:
                    print('.',end='',flush=True)
            Ns.append(n)
            Ws.append(W/cnt)
            Ts.append(T/cnt)
            print(W/cnt,T/cnt)
        for n in range(100,4500,250):
            print(n,end=' ',flush=True)
            W=T=0
            cnt=20
            for i in range(cnt):
                test(random_data(n))
                if i%2==0:
                    print('.',end='',flush=True)
            Ns.append(n)
            Ws.append(W/cnt)
            Ts.append(T/cnt)
            print(W/cnt,T/cnt)
        fig,(ax1, ax2) = plt.subplots(nrows=2)    
        ax1.plot(Ns,Ts)
        ax1.set_title('time')
        ax1.set_xlabel('n')
        ax2.plot(Ns,Ws)
        ax2.set_title('work')
        ax2.set_xlabel('n')
        plt.subplots_adjust(hspace=1)
        plt.savefig('upper_hull.png')
        print('%d failures'%faults)
        plt.show()

            

