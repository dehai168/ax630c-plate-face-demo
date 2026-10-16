import sys,json,csv,collections
sys.path.insert(0,'/workspace/cn_val_su/eval')
from gt import load_gt;from common import *
def lev(a,b):
    d=list(range(len(b)+1))
    for i in range(1,len(a)+1):
        p=d[:];d[0]=i
        for j in range(1,len(b)+1): d[j]=min(p[j]+1,d[j-1]+1,p[j-1]+(a[i-1]!=b[j-1]))
    return d[-1]
def score(res,g=None,thr=0.5):
    g=g or load_gt();rows=[];det=collections.Counter()
    for f,v in g.items():
        r=res.get(f,{'plates':[]});gp=[p['poly'] for p in v['plates']];pp=[q.get('kps',[]) for q in r['plates']]
        pp_ok=[i for i,q in enumerate(pp) if len(q)==4]
        m,ug,up=match(gp,[pp[i] for i in pp_ok],thr);m=[(a,pp_ok[b],i) for a,b,i in m];up=[pp_ok[i] for i in up]
        det['tp']+=len(m);det['fn']+=len(ug);det['fp']+=len(up);det['gt']+=len(gp);det['pred']+=len(pp_ok)
        for gi,pi,iou in m:
            G=v['plates'][gi];P=r['plates'][pi];gt=G.get('text','');pt=P['text']
            rows.append(dict(file=f,status=v['status'],matched=1,iou=round(iou,3),gt_text=gt,pred_text=pt,exact=int(bool(gt) and gt==pt),
              prov_ok=int(bool(gt) and pt[:1]==gt[:1]),edit=lev(gt,pt) if gt else '',gt_len=len(gt),
              gt_color=G.get('color',''),pred_color=P['color'],color_ok=int(G.get('color')==P['color']),
              gt_layer=G.get('layer',''),pred_layer='double' if P.get('final_layer',P['layer'])==1 else 'single',
              layer_ok=int(G.get('layer')==('double' if P.get('final_layer',P['layer'])==1 else 'single')),
              det_score=P['det_score'],rec_conf=P['rec_conf'],valid=P['valid']))
        for gi in ug:
            G=v['plates'][gi];rows.append(dict(file=f,status=v['status'],matched=0,gt_text=G.get('text',''),gt_color=G.get('color',''),gt_layer=G.get('layer',''),gt_len=len(G.get('text','')),exact=0,prov_ok=0))
        for pi in up:
            P=r['plates'][pi];rows.append(dict(file=f,status=v['status'],matched=-1,pred_text=P['text'],pred_color=P['color'],det_score=P['det_score'],rec_conf=P['rec_conf'],valid=P['valid']))
    return rows,det
def summarize(rows,det):
    S={};gtr=[r for r in rows if r['matched']>=0];M=[r for r in rows if r['matched']==1]
    T=[r for r in gtr if r['gt_text']];MT=[r for r in M if r['gt_text']]
    S['gt_plates']=det['gt'];S['pred_plates']=det['pred'];S['tp']=det['tp'];S['fp']=det['fp'];S['fn']=det['fn']
    S['recall']=det['tp']/det['gt'];S['precision']=det['tp']/det['pred'] if det['pred'] else 0
    S['exact_e2e']=sum(r['exact'] for r in T)/len(T);S['exact_matched']=sum(r['exact'] for r in MT)/len(MT)
    S['prov_e2e']=sum(r['prov_ok'] for r in T)/len(T);S['prov_matched']=sum(r['prov_ok'] for r in MT)/len(MT)
    S['alnum_exact_matched']=sum(1 for r in MT if r['gt_text'][1:]==r['pred_text'][1:])/len(MT)
    S['char_acc_matched']=1-sum(r['edit'] for r in MT)/sum(r['gt_len'] for r in MT)
    S['char_acc_e2e']=1-(sum(r['edit'] for r in MT)+sum(r['gt_len'] for r in T if r['matched']==0))/sum(r['gt_len'] for r in T)
    CT=[r for r in M if r['gt_color']];S['color_acc']=sum(r['color_ok'] for r in CT)/len(CT)
    LT=[r for r in M if r['gt_layer']];S['layer_acc']=sum(r['layer_ok'] for r in LT)/len(LT)
    S['n_text']=len(T);S['n_matched_text']=len(MT)
    by={}
    for st in ['ok','format_fail','low_conf','no_det','empty_text']:
        a=[r for r in gtr if r['status']==st];at=[r for r in a if r['gt_text']];am=[r for r in a if r['matched']==1 and r['gt_text']]
        by[st]=dict(gt=len(a),det=sum(r['matched']==1 for r in a),exact=sum(r['exact'] for r in at),prov=sum(r['prov_ok'] for r in at),n=len(at),fp=sum(1 for r in rows if r['matched']==-1 and r['status']==st))
    S['by_status']=by
    conf=collections.Counter((r['gt_text'][0],r['pred_text'][:1] or '∅') for r in MT if r['gt_text'][0]!=r['pred_text'][:1])
    S['prov_conf']=conf.most_common()
    # colour / layer confusion
    S['color_conf']=collections.Counter((r['gt_color'],r['pred_color']) for r in CT if not r['color_ok']).most_common()
    S['layer_conf']=collections.Counter((r['gt_layer'],r['pred_layer']) for r in LT if not r['layer_ok']).most_common()
    S['gt_prov_dist']=collections.Counter(r['gt_text'][0] for r in T).most_common()
    return S
def write_csv(rows,path):
    keys=['file','status','matched','iou','gt_text','pred_text','exact','prov_ok','edit','gt_len','gt_color','pred_color','color_ok','gt_layer','pred_layer','layer_ok','det_score','rec_conf','valid']
    w=csv.DictWriter(open(path,'w',newline='',encoding='utf-8-sig'),keys,extrasaction='ignore');w.writeheader();w.writerows(rows)
if __name__=='__main__':
    res=json.load(open(sys.argv[1]));rows,det=score(res);write_csv(rows,sys.argv[2]);S=summarize(rows,det)
    print(json.dumps(S,ensure_ascii=False,indent=1))
