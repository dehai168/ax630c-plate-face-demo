# 对指定帧跑车牌检测 onnx，打印候选 obj / cls 概率（与 C++ letterbox 一致：RGB/255，pad 0）
import cv2,numpy as np,onnxruntime as ort,sys
sess=ort.InferenceSession('/workspace/ax630c_plate_face_demo/models/onnx/plate_det_y5fu_384x640.onnx')
inp=sess.get_inputs()[0]; H,W=inp.shape[2],inp.shape[3]
anchors=[[4,5,8,10,13,16],[23,29,43,55,73,105],[146,217,231,300,335,433]]
sig=lambda x:1/(1+np.exp(-x))
def run(img):
    h,w=img.shape[:2]; s=min(W/w,H/h); nw,nh=int(round(w*s)),int(round(h*s)); px,py=(W-nw)//2,(H-nh)//2
    lb=np.zeros((H,W,3),np.uint8); lb[py:py+nh,px:px+nw]=cv2.resize(img,(nw,nh))
    x=lb[:,:,::-1].transpose(2,0,1)[None].astype(np.float32)/255
    outs=sess.run(None,{inp.name:x}); res=[]
    for o in outs:
        o=o[0]; gh,gw=o.shape[-2:]; st=H//gh; lv={8:0,16:1,32:2}[st]
        o=o.reshape(3,-1,gh,gw)
        for a in range(3):
            obj=sig(o[a,4]); ys,xs=np.where(obj>0.05)
            for y,xx in zip(ys,xs):
                c=sig(o[a,13:15,y,xx]); cx=(sig(o[a,0,y,xx])*2-.5+xx)*st; cy=(sig(o[a,1,y,xx])*2-.5+y)*st
                bw=(sig(o[a,2,y,xx])*2)**2*anchors[lv][a*2]; bh=(sig(o[a,3,y,xx])*2)**2*anchors[lv][a*2+1]
                res.append((obj[y,xx],c[0],c[1],(cx-bw/2-px)/s,(cy-bh/2-py)/s,bw/s,bh/s,st))
    return res
for f in range(int(sys.argv[1]),int(sys.argv[2])+1):
    img=cv2.imread(f'diag/f4986/src_{f}.jpg'); r=run(img)
    r=[x for x in r if 800<x[3]<1800 and 350<x[4]<650]
    r.sort(key=lambda x:-x[0]*max(x[1],x[2]))
    for x in r[:2]: print(f, 'obj=%.3f c0=%.3f c1=%.3f score=%.3f box=%.0f,%.0f,%.0f,%.0f s%d'%(x[0],x[1],x[2],x[0]*max(x[1],x[2]),*x[3:]))
