#!/bin/sh
# AX630C 板端一键运行
#   ./run.sh [输入视频] [输出视频]        默认: test_video.mp4 -> out.mp4
#   NPU=npu2 ./run.sh ...                 切换 npu2 版本模型（默认 npu1）
#   PFD_VNPU=1 NPU=npu1 ./run.sh ...      开启虚拟 NPU 模式（只能跑 npu1 模型）
cd "$(dirname "$0")"
IN=${1:-test_video.mp4}
OUT=${2:-out.mp4}
NPU=${NPU:-npu1}
SNAP=${SNAP:-snaps}
# lib/ 下是随包的 FFmpeg 动态库（LGPL）；AX 运行时库(libax_engine 等)用板子系统自带的
export LD_LIBRARY_PATH=$PWD/lib:/soc/lib:/opt/lib:/usr/lib:${LD_LIBRARY_PATH}
# 板子系统里找不到 libax_engine.so 时，才退回随包的 SDK 库（V2.0.0_P7，可能与板子驱动版本不匹配）
FOUND=0
for d in /soc/lib /opt/lib /usr/lib /lib; do [ -f "$d/libax_engine.so" ] && FOUND=1; done
if [ $FOUND -eq 0 ] && [ -d lib_sdk_fallback ]; then
  echo "[run.sh] 系统中未找到 libax_engine.so，使用 lib_sdk_fallback/"
  export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$PWD/lib_sdk_fallback
fi
mkdir -p "$SNAP"
# v2: 有 demo.cfg 就加载（阈值/过滤/OSD 等参数）；CFG=none 不加载；EXTRA_ARGS 可覆盖；EXTRA_ARGS=--legacy 恢复 v1 行为对比
CFG=${CFG:-demo.cfg}
CFG_ARG=""; [ "$CFG" != "none" ] && [ -f "$CFG" ] && CFG_ARG="--config $CFG"
./plate_face_demo -i "$IN" -o "$OUT" \
  --face  models/face_det_${NPU}.axmodel \
  --plate models/plate_det_${NPU}.axmodel \
  --rec   models/plate_rec_${NPU}.axmodel \
  --color models/plate_cls_${NPU}.axmodel \
  --snap_dir "$SNAP" --result_json "${OUT%.*}_plates.json" $CFG_ARG $EXTRA_ARGS
