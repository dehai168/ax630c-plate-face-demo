#!/usr/bin/env python3
"""在 CCPD 图上用 GT 四角点拉正后评估识别 ONNX（只用于 sanity check，CCPD 很可能在 HyperLPR3 训练集中，不代表真实场景精度）
用法: python eval_rec_onnx.py --rec ../models/onnx/plate_rec_rpv3_48x160.onnx --dirs ccpd_dir --n 500 --expand 0.04,0.08
"""
import argparse, glob, os, sys
import cv2, numpy as np, onnxruntime as ort
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from ccpd_utils import parse
from prepare_calib_lib import rectify, rec_input
TOK = ["", "'"] + list("0123456789ABCDEFGHJKLMNOPQRSTUVWXYZ") + list("云京冀吉学宁川挂新晋桂民沪津浙渝港湘琼甘皖粤航苏蒙藏警豫贵赣辽鄂闽陕青鲁黑领使澳")
ap = argparse.ArgumentParser()
ap.add_argument('--rec', required=True); ap.add_argument('--dirs', nargs='+', required=True)
ap.add_argument('--n', type=int, default=500); ap.add_argument('--expand', default='0.04,0.08')
a = ap.parse_args()
s = ort.InferenceSession(a.rec)
files = []
for d in a.dirs: files += sorted(glob.glob(os.path.join(d, '*.jpg')))[:a.n]
for ex in a.expand.split(';'):
    exx, exy = map(float, ex.split(','))
    ok = okp = tot = 0
    for f in files:
        g = parse(f); img = cv2.imread(f)
        x = rec_input(rectify(img, g['corners'], exx, exy)).astype(np.float32)
        x = ((x - 127.5) / 127.5).transpose(2, 0, 1)[None]
        p = s.run(None, {'data': x})[0][0]
        idx = p.argmax(1); txt = ''; prev = -1
        for i in idx:
            if i != 0 and i != prev: txt += TOK[i]
            prev = i
        txt = txt.replace("'", '')
        tot += 1; ok += txt == g['text']; okp += txt[1:] == g['text'][1:]
    print(f'expand {exx},{exy}: full-match {ok}/{tot} = {ok/tot:.3f}, ignore-province {okp/tot:.3f}')
