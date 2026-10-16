import json,os
from collections import Counter
def load_gt(exp=None):
    exp=exp or os.environ.get('GT_EXPORT','/workspace/cn_val_su/cn_val_su_export_v2.json')
    d=json.load(open(exp)); gt={}
    for t in d:
        f=t['data']['file']; anns=[a for a in t['annotations'] if not a['was_cancelled']]
        plates=[]; nann=len(anns)
        if anns:
            a=anns[-1]; reg={}
            for r in a['result']:
                v=r['value']; i=r['id']; W=r['original_width'];H=r['original_height']
                e=reg.setdefault(i,{'W':W,'H':H})
                if r['type']=='polygonlabels': e['poly']=[(x*W/100,y*H/100) for x,y in v['points']]
                elif r['from_name']=='layer': e['layer']=v['choices'][0]
                elif r['from_name']=='color': e['color']=v['choices'][0]
                elif r['from_name']=='text': e['text']=v['text'][0].strip().replace(' ','') if v['text'] else ''
                elif r['type']=='rectanglelabels': e['rect']=v
            plates=[e for e in reg.values() if 'poly' in e]
        gt[f]=dict(plates=plates,status=t['data']['status'],group=t['data']['group'],nann=nann,skipped=(nann==0))
    return gt
