import json,tarfile,os,random,io
D='data';meta=json.load(open(D+'/meta.json'));rng=random.Random(3)
calib=[m for m in meta if m['split']=='calib']
orig=[m for m in calib if m['aug']==0];aug=[m for m in calib if m['aug']>0];rng.shuffle(aug)
real=orig+aug[:100]
held=set(json.load(open(D+'/split.json'))['held']);assert not any(m['file'] in held for m in real+orig)
assert len({m['name'] for m in meta})==len(meta)
def mk(name,kind,items,extra_tar=None):
    p=f'calib/{name}.tar'
    with tarfile.open(p,'w') as t:
        for i,m in enumerate(items):t.add(f'{D}/{kind}/{m["name"]}.png',arcname=f'{i:04d}.png')
        if extra_tar:
            with tarfile.open(extra_tar) as e:
                for j,mem in enumerate(e.getmembers()):
                    if mem.isfile():
                        b=e.extractfile(mem).read();ti=tarfile.TarInfo(f'c{j:04d}.png');ti.size=len(b);t.addfile(ti,io.BytesIO(b))
    print(p,len(items),bool(extra_tar))
for kind,nm in [('rec','plate_rec'),('cls','plate_cls')]:
    mk(f'{nm}_real',kind,real);mk(f'{nm}_mix',kind,orig,f'/workspace/dl/calib/{nm}_calib.tar')
R='/workspace/ax630c_plate_face_demo/scripts/pulsar2_configs/'
def cfg(nm,calib,method,u16,out):
    c=json.load(open(f'{R}{nm}_npu1.json'))
    c['quant']['input_configs'][0]['calibration_dataset']=f'/workspace/cn_val_su/improve/calib/{calib}.tar'
    c['quant']['calibration_method']=method
    if u16:c['quant']['layer_configs']=[dict(start_tensor_names=['DEFAULT'],end_tensor_names=['DEFAULT'],data_type='U16')]
    else:c['quant'].pop('layer_configs',None)
    json.dump(c,open(f'build/{out}.json','w'),indent=1)
cfg('plate_rec','plate_rec_real','MinMax',True,'rec_real_minmax')
cfg('plate_rec','plate_rec_mix','MinMax',True,'rec_mix_minmax')
cfg('plate_rec','plate_rec_mix','MSE',True,'rec_mix_mse')
cfg('plate_rec','plate_rec_mix','Percentile',True,'rec_mix_pct')
cfg('plate_cls','plate_cls_real','MinMax',False,'cls_real_i8')
cfg('plate_cls','plate_cls_real','MinMax',True,'cls_real_u16')
cfg('plate_cls','plate_cls_mix','MSE',False,'cls_mix_mse_i8')
