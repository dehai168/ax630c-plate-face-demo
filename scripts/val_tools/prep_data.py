import sys,json,os,hashlib,random,tarfile,collections
sys.path.insert(0,'/workspace/cn_val_su/eval');sys.path.insert(0,'/workspace/ax630c_plate_face_demo/scripts')
import cv2,numpy as np
from gt import load_gt
from prepare_calib_lib import rectify,rec_input
D='/workspace/cn_val_su/improve/data';IMG='/workspace/cn_val_su/raw/images/'
g=load_gt();files=sorted(g,key=lambda f:hashlib.md5(f.encode()).hexdigest())
# stratified-ish: held-out = every 3rd of hash-sorted -> 100 imgs
held=set(files[::3]);calib=[f for f in files if f not in held]
json.dump(dict(held=sorted(held),calib=sorted(calib)),open(D+'/split.json','w'),indent=1)
def order(p):
    p=np.float32(p);s=p.sum(1);d=p[:,0]-p[:,1]
    tl=p[s.argmin()];br=p[s.argmax()];tr=p[d.argmax()];bl=p[d.argmin()]
    return np.float32([tl,tr,br,bl])
def merge_double(img):
    h,w=img.shape[:2];up=img[:max(1,h*5//12)];down=img[h//3:]
    up=cv2.resize(up,(down.shape[1],down.shape[0]));return np.hstack([up,down])
def rot(o):
    return cv2.rotate(o,cv2.ROTATE_90_COUNTERCLOCKWISE) if o.shape[0]>=o.shape[1]*1.5 else o
meta=[];rng=random.Random(1)
for f in files:
    im=cv2.imread(IMG+f)
    for pi,pl in enumerate(g[f]['plates']):
        P=order(pl['poly']);
        variants=[P]
        if f in calib:  # jitter augmentation for calibration only
            for _ in range(2):
                wd=np.linalg.norm(P[1]-P[0]);variants.append(P+np.float32([[rng.uniform(-.03,.03)*wd,rng.uniform(-.03,.03)*wd] for _ in range(4)]))
        for vi,V in enumerate(variants):
            rc=rot(rectify(im,V))
            if rc.size==0 or min(rc.shape[:2])<4: continue
            dbl=pl.get('layer')=='double'
            ri=rec_input(merge_double(rc) if dbl else rc)
            ci=cv2.resize(rc,(96,96))
            name=f'{hashlib.md5(f.encode()).hexdigest()[:10]}_{pi}_{vi}'
            os.makedirs(D+'/rec',exist_ok=True);os.makedirs(D+'/cls',exist_ok=True)
            cv2.imwrite(f'{D}/rec/{name}.png',ri);cv2.imwrite(f'{D}/cls/{name}.png',ci)
            meta.append(dict(name=name,file=f,split='held' if f in held else 'calib',aug=vi,text=pl.get('text',''),color=pl.get('color',''),layer=pl.get('layer',''),w=rc.shape[1],h=rc.shape[0]))
json.dump(meta,open(D+'/meta.json','w'),ensure_ascii=False,indent=0)
c=collections.Counter((m['split'],m['aug']>0) for m in meta);print(c)
print(collections.Counter((m['split'],m['color']) for m in meta if m['aug']==0))
print(collections.Counter((m['split'],m['layer']) for m in meta if m['aug']==0))
