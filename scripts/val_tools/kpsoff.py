import json,sys,numpy as np
sys.path.insert(0,'/workspace/cn_val_su/eval')
from common import *;from parse_eval import parse
h=set(json.load(open('data/split.json'))['held'])
x=parse(open('t1_d1.tsv',errors='ignore').read())
for t in sys.argv[1:]:
    b=parse(open(t,errors='ignore').read());io=[];off=[];n=0;nb=0
    for f in x:
        if f not in h:continue
        P=[p for p in x[f]['plates'] if len(p.get('kps',[]))==4];Q=[p for p in b.get(f,{'plates':[]})['plates'] if len(p.get('kps',[]))==4]
        n+=len(P);nb+=len(Q)
        m,_,_=match([p['kps'] for p in P],[p['kps'] for p in Q],0.3)
        for gi,pi,i in m:
            io.append(i);k1=np.array(P[gi]['kps']);k2=np.array(Q[pi]['kps']);off.append(np.linalg.norm(k1-k2,axis=1).mean()/np.linalg.norm(k1[1]-k1[0]))
    print(t.split('/')[-1],'matched',len(io),'/',n,'x86 dets;board dets',nb,'IoU %.3f offset %.4f'%(np.mean(io),np.mean(off)))
