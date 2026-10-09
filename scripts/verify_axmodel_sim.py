#!/usr/bin/env python3
"""用 Pulsar2 x86 仿真器 (pulsar2 run) 跑编译后的 axmodel，与 ONNX(FP32) 结果对比，确认 INT8/U16 量化后的
检测/识别结果在 PC 上是否基本一致（板上 NPU 与 pulsar2 run 结果按官方文档应一致，但本脚本无法替代上板验证）。
- plate_rec: CCPD 图按 GT 四角点拉正 -> 识别，统计 ONNX / axmodel 各自与 GT 的整牌一致率、二者互相一致率
- plate_cls: 颜色三分类 ONNX vs axmodel 一致率
- plate_det / face_det: letterbox 后的检测，按与 ONNX 结果 IoU>0.5 的匹配率 + 角点/关键点平均偏差(px)
用法: python verify_axmodel_sim.py --pulsar2 <pulsar2> --models ../models --ccpd_dir d --face_imgs a.jpg ... --npu npu1 --n 100
"""
import argparse, glob, json, os, shutil, subprocess, sys
import cv2, numpy as np, onnxruntime as ort
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from ccpd_utils import parse
from prepare_calib_lib import rectify, rec_input, letterbox

TOK = ["", "'"] + list("0123456789ABCDEFGHJKLMNOPQRSTUVWXYZ") + list("云京冀吉学宁川挂新晋桂民沪津浙渝港湘琼甘皖粤航苏蒙藏警豫贵赣辽鄂闽陕青鲁黑领使澳")
ANCH = np.array([4, 5, 8, 10, 13, 16, 23, 29, 43, 55, 73, 105, 146, 217, 231, 300, 335, 433], np.float32).reshape(3, 3, 2)
sig = lambda x: 1 / (1 + np.exp(-x))

ap = argparse.ArgumentParser()
ap.add_argument('--pulsar2', required=True)
ap.add_argument('--models', required=True)
ap.add_argument('--ccpd_dir', nargs='+', required=True)
ap.add_argument('--face_imgs', nargs='+', required=True)
ap.add_argument('--npu', default='npu1')
ap.add_argument('--n', type=int, default=100)
ap.add_argument('--work', default='/tmp/axsim')
ap.add_argument('--only', default='plate_rec,plate_cls,plate_det,face_det')
a = ap.parse_args()
os.makedirs(a.work, exist_ok=True)

def sim_run(name, axmodel, in_name, arrays, out_names):
    d = os.path.join(a.work, name); shutil.rmtree(d, ignore_errors=True)
    ind, outd = os.path.join(d, 'in'), os.path.join(d, 'out')
    for i, x in enumerate(arrays):
        os.makedirs(os.path.join(ind, str(i)), exist_ok=True)
        x.astype(np.uint8).tofile(os.path.join(ind, str(i), in_name + '.bin'))
    with open(os.path.join(d, 'list.txt'), 'w') as f: f.write('\n'.join(str(i) for i in range(len(arrays))))
    r = subprocess.run([a.pulsar2, 'run', '--model', axmodel, '--input_dir', ind, '--output_dir', outd,
                        '--list', os.path.join(d, 'list.txt')], capture_output=True, text=True)
    if r.returncode != 0: print(r.stdout[-2000:], r.stderr[-2000:]); raise SystemExit('pulsar2 run failed')
    res = []
    for i in range(len(arrays)):
        res.append({o: np.fromfile(os.path.join(outd, str(i), o + '.bin'), np.float32) for o in out_names})
    return res

def onnx_run(path, arrays, mean, std, to_rgb):
    s = ort.InferenceSession(path); inn = s.get_inputs()[0].name
    outs = []
    for x in arrays:
        x = x[..., ::-1] if to_rgb else x
        t = ((x.astype(np.float32) - mean) / std).transpose(2, 0, 1)[None]
        outs.append({o.name: v.ravel() for o, v in zip(s.get_outputs(), s.run(None, {inn: t}))})
    return outs

def ctc(p):
    idx = p.argmax(1); t = ''; prev = -1
    for i in idx:
        if i != 0 and i != prev: t += TOK[i]
        prev = i
    return t.replace("'", '')

def decode(outs, shapes, nk, nc, thr=0.4, W=640, H=384):
    dets = []
    no = 5 + 2 * nk + nc
    for l, (name, s) in enumerate(zip(['p8', 'p16', 'p32'], [8, 16, 32])):
        gh, gw = H // s, W // s
        t = outs[name].reshape(3, no, gh, gw)
        for ai in range(3):
            sc = sig(t[ai, 4]) * sig(t[ai, 5 + 2 * nk:].max(0))
            for yy, xx in zip(*np.where(sc > thr)):
                p = t[ai, :, yy, xx]
                cx = (sig(p[0]) * 2 - .5 + xx) * s; cy = (sig(p[1]) * 2 - .5 + yy) * s
                w = (sig(p[2]) * 2) ** 2 * ANCH[l, ai, 0]; h = (sig(p[3]) * 2) ** 2 * ANCH[l, ai, 1]
                k = p[5:5 + 2 * nk].reshape(nk, 2) * ANCH[l, ai] + np.array([xx * s, yy * s])
                dets.append((sc[yy, xx], np.array([cx - w / 2, cy - h / 2, cx + w / 2, cy + h / 2]), k))
    dets.sort(key=lambda d: -d[0]); keep = []
    for d in dets:
        if all(iou(d[1], k[1]) < 0.45 for k in keep): keep.append(d)
    return keep

def iou(a_, b):
    x1, y1 = max(a_[0], b[0]), max(a_[1], b[1]); x2, y2 = min(a_[2], b[2]), min(a_[3], b[3])
    i = max(0, x2 - x1) * max(0, y2 - y1)
    return i / ((a_[2] - a_[0]) * (a_[3] - a_[1]) + (b[2] - b[0]) * (b[3] - b[1]) - i + 1e-9)

files = []
for d in a.ccpd_dir: files += sorted(glob.glob(os.path.join(d, '*.jpg')))[:a.n]
gts = [parse(f) for f in files]
imgs = [cv2.imread(f) for f in files]
M = lambda n: os.path.join(a.models, 'axmodel', f'{n}_{a.npu}.axmodel')
O = lambda n: os.path.join(a.models, 'onnx', n)
report = {}
only = a.only.split(',')

if 'plate_rec' in only:
    xs = [rec_input(rectify(im, g['corners'])) for im, g in zip(imgs, gts)]
    on = onnx_run(O('plate_rec_rpv3_48x160.onnx'), xs, 127.5, 127.5, False)
    ax = sim_run('rec', M('plate_rec'), 'data', xs, ['output'])
    to = [ctc(o['output'].reshape(20, 78)) for o in on]; ta = [ctc(o['output'].reshape(20, 78)) for o in ax]
    gt = [g['text'] for g in gts]
    report['plate_rec'] = dict(n=len(xs), onnx_acc=float(np.mean([x == y for x, y in zip(to, gt)])),
                               axmodel_acc=float(np.mean([x == y for x, y in zip(ta, gt)])),
                               onnx_vs_axmodel_agree=float(np.mean([x == y for x, y in zip(to, ta)])))
if 'plate_cls' in only:
    xs = [cv2.resize(rectify(im, g['corners']), (96, 96)) for im, g in zip(imgs, gts)]
    on = onnx_run(O('plate_cls_96x96.onnx'), xs, 0., 255., False)
    ax = sim_run('cls', M('plate_cls'), 'data', xs, ['output'])
    report['plate_cls'] = dict(n=len(xs), onnx_vs_axmodel_agree=float(np.mean([o['output'].argmax() == x['output'].argmax() for o, x in zip(on, ax)])))

def det_compare(key, onnx_name, ax_name, arrays, nk, nc, gt_boxes=None):
    on = onnx_run(O(onnx_name), arrays, 0., 255., True)
    # axmodel 输入为 RGB（src_format=RGB），这里与板端程序一样先 BGR->RGB
    ax = sim_run(key, M(ax_name), 'images', [x[..., ::-1].copy() for x in arrays], ['p8', 'p16', 'p32'])
    matched = tot = 0; kerr = []; gt_hit_o = gt_hit_a = 0
    for i, (o, x) in enumerate(zip(on, ax)):
        do, da = decode(o, None, nk, nc), decode(x, None, nk, nc)
        for d in do:
            tot += 1
            best = max(da, key=lambda e: iou(d[1], e[1]), default=None)
            if best is not None and iou(d[1], best[1]) > 0.5:
                matched += 1; kerr.append(float(np.abs(d[2] - best[2]).mean()))
        if gt_boxes is not None:
            g = gt_boxes[i]
            gt_hit_o += any(iou(d[1], g) > 0.5 for d in do); gt_hit_a += any(iou(d[1], g) > 0.5 for d in da)
    r = dict(n_images=len(arrays), onnx_dets=tot, axmodel_matched=matched,
             match_rate=matched / max(1, tot), mean_kp_abs_err_px=float(np.mean(kerr)) if kerr else None)
    if gt_boxes is not None:
        r.update(gt_recall_onnx=gt_hit_o / len(arrays), gt_recall_axmodel=gt_hit_a / len(arrays))
    report[key] = r

if 'plate_det' in only:
    arr, gtb = [], []
    for im, g in zip(imgs, gts):
        lb = letterbox(im, 640, 384, 0); arr.append(lb)
        r = min(640 / im.shape[1], 384 / im.shape[0]); px = (640 - round(im.shape[1] * r)) // 2; py = (384 - round(im.shape[0] * r)) // 2
        x1, y1, x2, y2 = g['box']; gtb.append(np.array([x1 * r + px, y1 * r + py, x2 * r + px, y2 * r + py]))
    det_compare('plate_det', 'plate_det_y5fu_384x640.onnx', 'plate_det', arr, 4, 2, gtb)
if 'face_det' in only:
    arr = []
    for fp in a.face_imgs:
        im = cv2.imread(fp); h, w = im.shape[:2]
        arr.append(letterbox(im, 640, 384, 114))
        for s in (0.5, 0.7):
            for (x, y) in ((0, 0), (w - int(w * s), h - int(h * s))):
                arr.append(letterbox(im[y:y + int(h * s), x:x + int(w * s)], 640, 384, 114))
    det_compare('face_det', 'face_det_yolov5n_384x640.onnx', 'face_det', arr, 5, 1)
print(json.dumps(report, indent=1, ensure_ascii=False))
