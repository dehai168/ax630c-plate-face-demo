import sys,os,json,shutil,subprocess,argparse
import cv2,numpy as np,onnxruntime as ort
ap=argparse.ArgumentParser();ap.add_argument('--rec');ap.add_argument('--cls');ap.add_argument('--split',default='held');ap.add_argument('--out');ap.add_argument('--work',default='/tmp/simw')
a=ap.parse_args()
P2='/workspace/dl/pulsar2/ax_pulsar2_8.0_lite_package/bin/pulsar2_fixed'
D='/workspace/cn_val_su/improve/data';OX='/workspace/ax630c_plate_face_demo/models/onnx/'
TOK=["","'"]+list("0123456789ABCDEFGHJKLMNOPQRSTUVWXYZ")+list("云京冀吉学宁川挂新晋桂民沪津浙渝港湘琼甘皖粤航苏蒙藏警豫贵赣辽鄂闽陕青鲁黑领使澳")
meta=[m for m in json.load(open(D+'/meta.json')) if m['split']==a.split and m['aug']==0][:int(os.environ.get('NMAX','999'))]
def ctc(p):
    idx=p.argmax(1);t='';prev=-1
    for i in idx:
        if i!=0 and i!=prev:t+=TOK[i]
        prev=i
    return t.replace("'","")
def ort_run(path,xs,mean,std):
    s=ort.InferenceSession(path);n=s.get_inputs()[0].name;o=[]
    for x in xs:o.append(s.run(None,{n:((x.astype(np.float32)-mean)/std).transpose(2,0,1)[None]})[0].ravel())
    return o
def sim(name,model,xs,nproc=7):
    from concurrent.futures import ThreadPoolExecutor
    d=f'{a.work}/{name}';shutil.rmtree(d,ignore_errors=True)
    for i,x in enumerate(xs):
        os.makedirs(f'{d}/in/{i}',exist_ok=True);x.astype(np.uint8).tofile(f'{d}/in/{i}/data.bin')
    chunks=[list(range(k,len(xs),nproc)) for k in range(nproc)]
    def job(k):
        ids=chunks[k]
        if not ids:return
        open(f'{d}/list{k}.txt','w').write('\n'.join(map(str,ids)))
        r=subprocess.run([P2,'run','--model',model,'--input_dir',d+'/in','--output_dir',d+'/out','--list',f'{d}/list{k}.txt'],capture_output=True,text=True)
        if r.returncode:print(r.stdout[-1500:],r.stderr[-1500:]);sys.exit(1)
    with ThreadPoolExecutor(nproc) as ex:list(ex.map(job,range(nproc)))
    return [np.fromfile(f'{d}/out/{i}/output.bin',np.float32) for i in range(len(xs))]
res={}
if a.rec:
    xs=[cv2.imread(f'{D}/rec/{m["name"]}.png') for m in meta]
    o=ort_run(OX+'plate_rec_rpv3_48x160.onnx',xs,127.5,127.5);x=sim('rec',a.rec,xs)
    to=[ctc(v.reshape(20,-1)) for v in o];tx=[ctc(v.reshape(20,-1)) for v in x]
    gt=[m['text'] for m in meta];idx=[i for i,t in enumerate(gt) if t]
    cl=lambda v:v.reshape(20,-1).max(1)  # max logit proxy
    res['rec']=dict(n=len(idx),ort_exact=np.mean([to[i]==gt[i] for i in idx]),ax_exact=np.mean([tx[i]==gt[i] for i in idx]),agree=np.mean([to[i]==tx[i] for i in idx]),
      ort_prov=np.mean([to[i][:1]==gt[i][:1] for i in idx]),ax_prov=np.mean([tx[i][:1]==gt[i][:1] for i in idx]),
      mean_abs_diff=float(np.mean([np.abs(o[i]-x[i]).mean() for i in idx])),
      cos=float(np.mean([np.dot(o[i],x[i])/np.linalg.norm(o[i])/np.linalg.norm(x[i]) for i in idx])),
      diffs=[(gt[i],to[i],tx[i]) for i in idx if to[i]!=tx[i] or tx[i]!=gt[i]])
if a.cls:
    xs=[cv2.imread(f'{D}/cls/{m["name"]}.png') for m in meta]
    o=ort_run(OX+'plate_cls_96x96.onnx',xs,0,255.);x=sim('cls',a.cls,xs)
    cn=['蓝','绿','黄'];gtc=[m['color'] for m in meta];idx=[i for i,c in enumerate(gtc) if c in cn]
    res['cls']=dict(n=len(idx),ort_acc=np.mean([cn[o[i].argmax()]==gtc[i] for i in idx]),ax_acc=np.mean([cn[x[i].argmax()]==gtc[i] for i in idx]),agree=np.mean([o[i].argmax()==x[i].argmax() for i in idx]),
      mean_abs_diff=float(np.mean([np.abs(o[i]-x[i]).mean() for i in idx])),diffs=[(gtc[i],cn[o[i].argmax()],cn[x[i].argmax()],[round(float(v),3) for v in o[i]],[round(float(v),3) for v in x[i]]) for i in idx if o[i].argmax()!=x[i].argmax() or cn[x[i].argmax()]!=gtc[i]])
json.dump(res,open(a.out,'w'),ensure_ascii=False,indent=1,default=float)
for k,v in res.items():print(k,{kk:(round(vv,4) if isinstance(vv,float) else vv) for kk,vv in v.items() if kk!='diffs'});[print('  ',d) for d in v['diffs']]
