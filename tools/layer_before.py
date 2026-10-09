# 统计 v2.0 叠加视频中显示 "(双)" 的帧/轨迹（v2.0 按每帧检测 cls 显示），用 dets.txt 的原始检测匹配 SP 框
import collections,json,sys
dets=collections.defaultdict(list)
for l in open('diag/dets.txt'):
    p=l.split()
    if p[0]=='P': dets[int(p[1])].append((int(p[3]),*map(float,p[4:8])))
def iou(a,b):
    ax,ay,aw,ah=a; bx,by,bw,bh=b
    ix=max(0,min(ax+aw,bx+bw)-max(ax,bx)); iy=max(0,min(ay+ah,by+bh)-max(ay,by)); i=ix*iy
    return i/(aw*ah+bw*bh-i+1e-6)
fr=collections.Counter(); trk=collections.defaultdict(lambda:[0,0,set()])
for l in open(sys.argv[1],encoding='utf-8'):
    p=l.split()
    if p[0]!='SP': continue
    f=int(p[1]); box=tuple(map(float,p[5:9])); txt=p[9]
    best=max(dets.get(f,[]),key=lambda d:iou(d[1:],box),default=None)
    cls=best[0] if best and iou(best[1:],box)>0.8 else -1
    t=trk[int(p[3])]; t[0]+=1; t[1]+=cls==1; 
    if txt!='-': t[2].add(txt)
    fr[cls]+=1
print('SP frames by det cls:',dict(fr))
col={}
for j in json.load(open(sys.argv[2]))['plates']: 
    for t in j['tracks']: col[j['plate']]=j['color']
dbl=[(k,v[0],v[1],sorted(v[2])) for k,v in trk.items() if v[1]>0]
print('display tracks with any (双) frame:',len(dbl),'of',len(trk))
c=collections.Counter()
for k,n,d,txts in dbl:
    c[col.get(txts[0],'?') if txts else '?']+=1
print('by color:',dict(c))
for x in sorted(dbl,key=lambda x:-x[2])[:40]: print(x)
