#!/bin/sh
# AX630C 板端一键运行
#   ./run.sh [输入视频] [输出视频]        默认: test_video.mp4 -> out.mp4
#   NPU=npu2 ./run.sh ...                 切换 npu2 版本模型（默认 npu1）
#   PFD_VNPU=0 ./run.sh ...               关闭虚拟 NPU 模式（默认 1：固件已把 NPU 设为虚拟模式时必须开，只能跑 npu1 模型）
#   PLATE_MODEL=models/xxx.axmodel ./run.sh   指定车牌检测模型（v2.4 默认 npu1 用 plate_det_npu1_u16.axmodel）
cd "$(dirname "$0")"
IN=${1:-test_video.mp4}
OUT=${2:-out.mp4}
NPU=${NPU:-npu1}
export PFD_VNPU=${PFD_VNPU:-1}   # v2.4: 默认开启虚拟 NPU（AX630C 出厂固件已设为虚拟模式，否则 AX_ENGINE_Init 失败 0x80060087）
if [ -z "$PLATE_MODEL" ]; then   # v2.4: npu1 默认用 U16 车牌检测模型（角点精度更高）
  PLATE_MODEL=models/plate_det_${NPU}.axmodel
  [ "$NPU" = "npu1" ] && [ -f models/plate_det_npu1_u16.axmodel ] && PLATE_MODEL=models/plate_det_npu1_u16.axmodel
fi
SNAP=${SNAP:-snaps}
echo "[run.sh] plate_face_demo v2.4  NPU=$NPU PFD_VNPU=$PFD_VNPU plate=$PLATE_MODEL"
# lib/ 下是随包的 FFmpeg 动态库（LGPL）；AX 运行时库(libax_engine 等)用板子系统自带的
export LD_LIBRARY_PATH=/usr/local/lib:$PWD/lib:/soc/lib:/opt/lib:/usr/lib:${LD_LIBRARY_PATH}
# 板子系统里找不到 libax_engine.so 时，才退回随包的 SDK 库（V2.0.0_P7，可能与板子驱动版本不匹配）
FOUND=0
for d in /usr/local/lib /soc/lib /opt/lib /usr/lib /lib; do [ -f "$d/libax_engine.so" ] && FOUND=1; done
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
  --plate "$PLATE_MODEL" \
  --rec   models/plate_rec_${NPU}.axmodel \
  --color models/plate_cls_${NPU}.axmodel \
  --snap_dir "$SNAP" --result_json "${OUT%.*}_plates.json" $CFG_ARG $EXTRA_ARGS
