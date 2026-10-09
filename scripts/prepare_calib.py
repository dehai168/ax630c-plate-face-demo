#!/usr/bin/env python3
"""生成 Pulsar2 INT8 量化校准集（tar 包，图片已预处理到模型输入尺寸，BGR jpg/png）
  face_calib.tar      : 人脸图 letterbox 到 640x384（填充 114）
  plate_det_calib.tar : CCPD 图 letterbox 到 640x384（填充 0，与 HyperLPR3 一致）
  plate_rec_calib.tar : 按 CCPD GT 四角点拉正、等比缩放到高 48、右侧补 128 到 160 宽
  plate_cls_calib.tar : 拉正车牌 resize 96x96
注意：校准图应尽量来自目标场景（车载外场实拍），这里只是用公开数据跑通流程。
用法: python prepare_calib.py --ccpd_dirs d1 d2 --face_imgs a.jpg b.jpg --out calib/ --num 64
"""
import argparse, glob, os, random, sys, tarfile
import cv2, numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from ccpd_utils import parse

ap = argparse.ArgumentParser()
ap.add_argument('--ccpd_dirs', nargs='+', required=True)
ap.add_argument('--face_imgs', nargs='+', required=True)
ap.add_argument('--out', required=True)
ap.add_argument('--num', type=int, default=64)
ap.add_argument('--det_w', type=int, default=640)
ap.add_argument('--det_h', type=int, default=384)
a = ap.parse_args()
rng = random.Random(0)
os.makedirs(a.out, exist_ok=True)

def letterbox(img, W, H, pad):
    r = min(W / img.shape[1], H / img.shape[0])
    nw, nh = int(round(img.shape[1] * r)), int(round(img.shape[0] * r))
    out = np.full((H, W, 3), pad, np.uint8)
    px, py = (W - nw) // 2, (H - nh) // 2
    out[py:py + nh, px:px + nw] = cv2.resize(img, (nw, nh))
    return out

def rectify(img, corners, ex=0.02, ey=0.04):
    p = np.float32(corners)
    ux = ((p[1] - p[0]) + (p[2] - p[3])) / 2; uy = ((p[3] - p[0]) + (p[2] - p[1])) / 2
    s = np.float32([[-1, -1], [1, -1], [1, 1], [-1, 1]])
    p = p + ux[None] * (s[:, :1] * ex / 2) + uy[None] * (s[:, 1:] * ey / 2)
    w = int(max(np.linalg.norm(p[0] - p[1]), np.linalg.norm(p[2] - p[3])))
    h = int(max(np.linalg.norm(p[0] - p[3]), np.linalg.norm(p[1] - p[2])))
    M = cv2.getPerspectiveTransform(p, np.float32([[0, 0], [w, 0], [w, h], [0, h]]))
    return cv2.warpPerspective(img, M, (w, h), borderMode=cv2.BORDER_REPLICATE)

def rec_input(plate, H=48, W=160):
    rw = min(W, max(1, int(np.ceil(H * plate.shape[1] / plate.shape[0]))))
    out = np.full((H, W, 3), 128, np.uint8)
    out[:, :rw] = cv2.resize(plate, (rw, H))
    return out

def write_tar(name, imgs):
    d = os.path.join(a.out, name); os.makedirs(d, exist_ok=True)
    paths = []
    for i, im in enumerate(imgs):
        p = os.path.join(d, f'{i:04d}.png'); cv2.imwrite(p, im); paths.append(p)
    with tarfile.open(os.path.join(a.out, name + '.tar'), 'w') as t:
        for p in paths: t.add(p, arcname=os.path.basename(p))
    print(name, len(imgs))

files = []
for d in a.ccpd_dirs:
    fs = sorted(glob.glob(os.path.join(d, '*.jpg'))); rng.shuffle(fs); files += fs[:a.num]
rng.shuffle(files)
det, rec, cls = [], [], []
for f in files[:a.num * 2]:
    g = parse(f); img = cv2.imread(f)
    if g is None or img is None: continue
    if len(det) < a.num: det.append(letterbox(img, a.det_w, a.det_h, 0))
    pl = rectify(img, g['corners'])
    rec.append(rec_input(pl)); cls.append(cv2.resize(pl, (96, 96)))
faces = []
for fp in a.face_imgs:
    img = cv2.imread(fp)
    faces.append(letterbox(img, a.det_w, a.det_h, 114))
    h, w = img.shape[:2]
    while len(faces) < a.num * (a.face_imgs.index(fp) + 1) // len(a.face_imgs):
        s = rng.uniform(0.35, 0.9); cw, ch = int(w * s), int(h * s)
        x, y = rng.randint(0, w - cw), rng.randint(0, h - ch)
        faces.append(letterbox(img[y:y + ch, x:x + cw], a.det_w, a.det_h, 114))
write_tar('face_calib', faces[:a.num])
write_tar('plate_det_calib', det[:a.num])
write_tar('plate_rec_calib', rec[:a.num])
write_tar('plate_cls_calib', cls[:a.num])
