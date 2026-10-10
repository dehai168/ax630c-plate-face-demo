import sys,collections,os,re
def parse(f):
    R={}; img=None; cur=None
    for l in open(f,encoding='utf-8'):
        p=l.rstrip('\n').split('\t')
        if p[0]=='IMG':
            img=p[1]; R[img]={'read_fail':len(p)>2 and p[2]=='READ_FAIL','best':None}; cur=None
        elif p[0]=='PLATE':
            cur=dict(text=p[2],cls=int(p[3]),score=float(p[4]),color=p[5],color_conf=float(p[6]),conf=float(p[8]),valid=int(p[9]),box=p[10])
            b=R[img]['best']
            if b is None or cur['score']>b['score']: R[img]['best']=cur
        elif p[0]=='LAYER' and cur is not None:
            cur.update(layer=int(p[3]),pre=p[5],clsc=p[6],src=p[7],strict=int(p[8]))
    return R
def ed(a,b):
    d=list(range(len(b)+1))
    for i,ca in enumerate(a,1):
        prev=d[0]; d[0]=i
        for j,cb in enumerate(b,1):
            cur=d[j]; d[j]=min(d[j]+1,d[j-1]+1,prev+(ca!=cb)); prev=cur
    return d[-1]
def gt_color(t):
    if t.endswith('警') or t.startswith('WJ'): return '白'
    if '使' in t or '领' in t: return '黑'
    if len(t)==8: return '绿'
    return '蓝'
def category(t):
    if t.startswith('WJ'): return '武警(WJ)'
    if t.endswith('警'): return '警用(白)'
    if '使' in t: return '使馆(黑,使)'
    if '领' in t: return '领馆(黑,领)'
    if len(t)==8: return '新能源小型(绿)' if t[2].isalpha() else '新能源大型(黄绿)'
    return '普通'
def load_gt():
    lst=[l.strip() for l in open('gt_sample_list.txt')]
    G={}
    for l in open('gt_sample.tsv',encoding='utf-8'):
        p=l.rstrip('\n').split('\t'); i=int(p[0])
        G[os.path.basename(lst[i])]=(p[1], len(p)>2 and p[2]=='X')
    return G
