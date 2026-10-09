#!/usr/bin/env python3
"""用开放授权图片合成一段「车载移动抓拍」风格的测试视频（带 GT）。
- 车牌: CCPD 数据集图片 (https://github.com/detectRecog/CCPD, MIT License)，文件名自带车牌号/四角点
- 人脸: NASA 宇航员合影 (Wikimedia Commons, 美国联邦政府作品 Public Domain)
每段 = 左半边 CCPD 图做推近+平移（模拟自车接近目标车），右半边人脸合影做平移/缩放；
另有整幅车牌段、整幅人脸段。输出 mp4 + gt.json（每段的帧范围与车牌号）。
用法: python make_test_video.py --ccpd_dirs dir1 dir2 --faces f1.jpg f2.jpg --out test.mp4 --segments 10
"""
import argparse, glob, json, os, random, sys
import cv2, numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from ccpd_utils import parse

ap = argparse.ArgumentParser()
ap.add_argument('--ccpd_dirs', nargs='+', required=True)
ap.add_argument('--faces', nargs='+', required=True)
ap.add_argument('--out', required=True)
ap.add_argument('--w', type=int, default=1280)
ap.add_argument('--h', type=int, default=720)
ap.add_argument('--fps', type=int, default=15)
ap.add_argument('--seg_sec', type=float, default=2.6)
ap.add_argument('--segments', type=int, default=12)
ap.add_argument('--per_dir', type=int, default=6)
ap.add_argument('--seed', type=int, default=7)
ap.add_argument('--subsets', default='ccpd_base,ccpd_fn,ccpd_rotate,ccpd_tilt,ccpd_weather', help='只从这些 CCPD 子集挑图（CCPD2020 绿牌无子集标记，按亮度/模糊度过滤）')
ap.add_argument('--gap_sec', type=float, default=1.4, help='段间黑场，需大于跟踪器丢失删除时间，避免硬切镜头时 ID 串段')
a = ap.parse_args()
rng = random.Random(a.seed)

cands = []
for d in a.ccpd_dirs:
    fs = sorted(glob.glob(os.path.join(d, '*.jpg')))
    rng.shuffle(fs)
    picked = []
    for f in fs:
        g = parse(f)
        if not g: continue
        x1, y1, x2, y2 = g['box']
        # 选「车牌不太小、不太糊」的图，模拟抓拍有效距离内的目标
        if (x2 - x1) < 70: continue
        if g['subset']:
            if g['subset'] not in a.subsets.split(','): continue
        elif g['blur'] > 10 or g['brightness'] < 60:
            continue
        picked.append((f, g))
        if len(picked) >= a.per_dir: break
    cands += picked
rng.shuffle(cands)
faces = [cv2.imread(f) for f in a.faces]

def crop_resize(img, cx, cy, cw, ch, W, H):
    # 以 (cx,cy) 为中心取 cw x ch 区域（越界补边），缩放到 W x H，并返回仿射用于坐标变换
    M = np.array([[W / cw, 0, W / 2 - cx * W / cw], [0, H / ch, H / 2 - cy * H / ch]], np.float32)
    out = cv2.warpAffine(img, M, (W, H), flags=cv2.INTER_LINEAR, borderMode=cv2.BORDER_CONSTANT, borderValue=(40, 40, 40))
    return out, M

def ease(t): return t * t * (3 - 2 * t)

W, H, fps = a.w, a.h, a.fps
n_seg_frames = int(a.seg_sec * fps)
vw = cv2.VideoWriter(a.out, cv2.VideoWriter_fourcc(*'mp4v'), fps, (W, H))
gt = {'fps': fps, 'size': [W, H], 'segments': [], 'sources': {
    'plates': 'CCPD (detectRecog/CCPD, MIT License)', 'faces': [os.path.basename(f) for f in a.faces] + ['NASA, Public Domain via Wikimedia Commons']}}
frame_idx = 0
layouts = ['split', 'split', 'plate_full', 'split', 'face_full']
for si in range(min(a.segments, len(cands))):
    f, g = cands[si]
    img = cv2.imread(f)
    layout = layouts[si % len(layouts)]
    face = faces[si % len(faces)]
    x1, y1, x2, y2 = g['box']; pcx, pcy = (x1 + x2) / 2, (y1 + y2) / 2
    ih, iw = img.shape[:2]
    fh, fw = face.shape[:2]
    seg = {'index': si, 'layout': layout, 'start_frame': frame_idx, 'end_frame': frame_idx + n_seg_frames - 1,
           'plate_text': g['text'] if layout != 'face_full' else None, 'ccpd_file': os.path.basename(f) if layout != 'face_full' else None,
           'face_image': os.path.basename(a.faces[si % len(faces)]) if layout != 'plate_full' else None}
    # 每段随机的人脸平移方向
    fx0, fx1 = rng.uniform(0.3, 0.45) * fw, rng.uniform(0.55, 0.7) * fw
    fz0, fz1 = rng.uniform(0.55, 0.7), rng.uniform(0.35, 0.5)
    for k in range(n_seg_frames):
        t = ease(k / max(1, n_seg_frames - 1))
        canvas = np.zeros((H, W, 3), np.uint8)
        if layout in ('split', 'plate_full'):
            PW = W // 2 if layout == 'split' else W
            # 推近: 视野从整幅 -> 车牌附近 (车牌宽占视野 ~1/4)
            ch0 = min(ih, iw * H / PW); ch1 = max((y2 - y1) * 7.0, H * 0.35)
            ch = ch0 + (ch1 - ch0) * t; cw = ch * PW / H
            cx = iw / 2 + (pcx - iw / 2) * t + 25 * np.sin(k / 6.0)
            cy = ih / 2 + (pcy - ih / 2) * t
            # 尽量让取景框留在图内（减少黑边）
            cx = min(max(cx, min(cw / 2, iw / 2)), max(iw - cw / 2, iw / 2))
            cy = min(max(cy, min(ch / 2, ih / 2)), max(ih - ch / 2, ih / 2))
            part, _ = crop_resize(img, cx, cy, cw, ch, PW, H)
            canvas[:, :PW] = part
        if layout in ('split', 'face_full'):
            FW = W // 2 if layout == 'split' else W
            x0 = W - FW
            ch = (fz0 + (fz1 - fz0) * t) * fh; cw = ch * FW / H
            cx = fx0 + (fx1 - fx0) * t; cy = fh * 0.45 + 10 * np.sin(k / 5.0)
            part, _ = crop_resize(face, cx, cy, cw, ch, FW, H)
            canvas[:, x0:] = part
        noise = np.random.default_rng(frame_idx).normal(0, 3, canvas.shape)
        canvas = np.clip(canvas.astype(np.float32) + noise, 0, 255).astype(np.uint8)
        vw.write(canvas)
        frame_idx += 1
    gt['segments'].append(seg)
    for _ in range(int(a.gap_sec * fps)):
        vw.write(np.full((H, W, 3), 16, np.uint8)); frame_idx += 1
vw.release()
with open(os.path.splitext(a.out)[0] + '_gt.json', 'w', encoding='utf-8') as fo:
    json.dump(gt, fo, ensure_ascii=False, indent=1)
print('frames', frame_idx, '->', a.out)
