#!/bin/sh
# usage: run_eval2.sh tag rec.axmodel cls.axmodel   (models in /opt/cn_val/m)
cd /opt/cn_val
M=/opt/demo_v23/ax630c_plate_face_demo_board/models; DET=$M/plate_det_npu1.axmodel; [ -n "$4" ] && DET=/opt/cn_val/m/$4
export PFD_VNPU=1 NO_FACE=1 LD_LIBRARY_PATH=/usr/local/lib:/opt/demo_v23/ax630c_plate_face_demo_board/lib
mkdir -p vis2 res
./image_eval_v24 list.txt vis2 $M/face_det_npu1.axmodel $DET /opt/cn_val/m/$2 /opt/cn_val/m/$3 0.35 > res/$1.tsv 2> res/$1.err
echo done > res/$1.done
