import numpy as np,cv2
def poly_iou(a,b):
    a=np.array(a,np.float32);b=np.array(b,np.float32)
    ia,_=cv2.intersectConvexConvex(a,b) if False else (0,0)
    # use mask rasterization for robustness
    m=int(max(a.max(),b.max()))+2
    ma=np.zeros((m,m),np.uint8);mb=np.zeros((m,m),np.uint8)
    cv2.fillPoly(ma,[a.astype(np.int32)],1);cv2.fillPoly(mb,[b.astype(np.int32)],1)
    i=(ma&mb).sum();u=(ma|mb).sum();return i/u if u else 0
def rect_iou(a,b):
    x1=max(a[0],b[0]);y1=max(a[1],b[1]);x2=min(a[2],b[2]);y2=min(a[3],b[3])
    i=max(0,x2-x1)*max(0,y2-y1);u=(a[2]-a[0])*(a[3]-a[1])+(b[2]-b[0])*(b[3]-b[1])-i;return i/u if u>0 else 0
def bbox(p):
    xs=[q[0] for q in p];ys=[q[1] for q in p];return (min(xs),min(ys),max(xs),max(ys))
def match(gtp,prp,thr=0.5):
    """greedy by IoU; returns list of (gi,pi,iou), unmatched g, unmatched p"""
    pairs=sorted(((poly_iou(g,p),gi,pi) for gi,g in enumerate(gtp) for pi,p in enumerate(prp)),reverse=True)
    ug=set(range(len(gtp)));up=set(range(len(prp)));out=[]
    for iou,gi,pi in pairs:
        if iou<thr: break
        if gi in ug and pi in up: out.append((gi,pi,iou));ug.discard(gi);up.discard(pi)
    return out,sorted(ug),sorted(up)
