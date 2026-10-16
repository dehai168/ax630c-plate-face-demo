import sys,json
sys.path.insert(0,'/workspace/cn_val_su/eval')
from parse_eval import parse;from score import *;from gt import load_gt
def run(tsv,subset='all'):
    res=parse(open(tsv,errors='ignore').read());g=load_gt()
    if subset=='held':
        h=set(json.load(open('/workspace/cn_val_su/improve/data/split.json'))['held']);g={k:v for k,v in g.items() if k in h}
    elif subset=='calib':
        h=set(json.load(open('/workspace/cn_val_su/improve/data/split.json'))['calib']);g={k:v for k,v in g.items() if k in h}
    rows,det=score(res,g);S=summarize(rows,det);return S,rows
KEYS=['recall','exact_e2e','exact_matched','prov_matched','char_acc_matched','color_acc','layer_acc']
if __name__=='__main__':
    for t in sys.argv[1:]:
        for sub in ['held','all']:
            S,_=run(t,sub);print(t.split('/')[-1],sub,'n=',S['n_text'],' '.join(f'{k}={S[k]*100:.1f}' for k in KEYS))
