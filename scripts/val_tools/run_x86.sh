#!/bin/bash
# usage: run_x86.sh out_prefix [model dir overrides via env REC/CLS]; env passes through (e.g. DUAL_HYP)
R=/workspace/ax630c_plate_face_demo; M=$R/models/onnx
OUT=$1; mkdir -p /tmp/vis_$$
NO_FACE=1 $R/build/x86/image_eval /workspace/cn_val_su/list_paths.txt /tmp/vis_$$ $M/face_det_yolov5n_384x640.onnx $M/plate_det_y5fu_384x640.onnx ${REC:-$M/plate_rec_rpv3_48x160.onnx} ${CLS:-$M/plate_cls_96x96.onnx} 0.35 > $OUT.tsv 2>/dev/null
python3 - <<PY
import sys,json;sys.path.insert(0,'/workspace/cn_val_su/eval')
from parse_eval import parse;from score import *
res=parse(open('$OUT.tsv',errors='ignore').read());json.dump(res,open('$OUT.json','w'),ensure_ascii=False)
rows,det=score(res);write_csv(rows,'$OUT.csv');S=summarize(rows,det);json.dump(S,open('$OUT.summary.json','w'),ensure_ascii=False,indent=1)
print({k:(round(v,4) if isinstance(v,float) else v) for k,v in S.items() if k in('recall','exact_e2e','exact_matched','prov_matched','char_acc_matched','color_acc','layer_acc')})
PY
rm -rf /tmp/vis_$$
