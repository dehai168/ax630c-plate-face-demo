import sys,json
def parse(txt):
    res={};cur=None
    for l in txt.split('\n'):
        t=l.split('\t')
        if t[0]=='IMG' and len(t)>=8: cur=t[1];res[cur]=dict(size=t[2],t_det=float(t[5]),t_rec=float(t[6]),plates=[])
        elif t[0]=='IMG': cur=None
        elif t[0]=='PLATE' and cur and len(t)>=12:
            res[cur]['plates'].append(dict(text=t[2],layer=int(t[3]),det_score=float(t[4]),color=t[5],color_conf=float(t[6]),rec_conf=float(t[8]),valid=int(t[9]),small=int(t[11]) if len(t)>11 else 0))
        elif t[0]=='KPS' and cur: res[cur]['plates'][int(t[1])]['kps']=[[float(v) for v in p.split(',')] for p in t[2].split(';')]
        elif t[0]=='LAYER' and cur: res[cur]['plates'][int(t[1])]['final_layer']=int(t[3])
    return res
if __name__=='__main__':
    json.dump(parse(open(sys.argv[1],errors='ignore').read()),open(sys.argv[2],'w'),ensure_ascii=False)
