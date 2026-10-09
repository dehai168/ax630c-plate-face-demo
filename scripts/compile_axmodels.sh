#!/usr/bin/env bash
# 用 Pulsar2 把 4 个 ONNX 编译成 AX620E (AX630C/AX620Q) 的 axmodel，npu1 / npu2 各一份
# 用法:
#   PULSAR2=/path/to/ax_pulsar2_x.y_lite_package/bin/pulsar2   (或 docker 里直接用 pulsar2)
#   CALIB_DIR=/path/to/calib  (scripts/prepare_calib.py 的输出)
#   ./scripts/compile_axmodels.sh [face_det plate_det plate_rec plate_cls] [npu1 npu2]
set -euo pipefail
export COLUMNS=${COLUMNS:-220}
ROOT=$(cd "$(dirname "$0")/.." && pwd)
PULSAR2=${PULSAR2:-pulsar2}
: "${CALIB_DIR:?set CALIB_DIR (prepare_calib.py 输出目录)}"
NAMES=${NAMES:-"face_det plate_det plate_rec plate_cls"}
NPUS=${NPUS:-"npu1 npu2"}
declare -A ONNX=(
  [face_det]=$ROOT/models/onnx/face_det_yolov5n_384x640.onnx
  [plate_det]=$ROOT/models/onnx/plate_det_y5fu_384x640.onnx
  [plate_rec]=$ROOT/models/onnx/plate_rec_rpv3_48x160.onnx
  [plate_cls]=$ROOT/models/onnx/plate_cls_96x96.onnx
)
OUT=$ROOT/models/axmodel; mkdir -p "$OUT"
WORK=${WORK:-$ROOT/build/pulsar2}; mkdir -p "$WORK"
for n in $NAMES; do
  for npu in $NPUS; do
    cfg=$WORK/${n}_${npu}.json
    sed "s#CALIB_DIR#${CALIB_DIR}#g" "$ROOT/scripts/pulsar2_configs/${n}_${npu}.json" > "$cfg"
    echo "==> $n ($npu)"
    rm -rf "$WORK/${n}_${npu}"
    "$PULSAR2" build --target_hardware AX620E --input "${ONNX[$n]}" --output_dir "$WORK/${n}_${npu}" \
      --config "$cfg" > "$WORK/${n}_${npu}.log" 2>&1 || { echo "FAILED: see $WORK/${n}_${npu}.log"; tail -30 "$WORK/${n}_${npu}.log"; exit 1; }
    cp "$WORK/${n}_${npu}/compiled.axmodel" "$OUT/${n}_${npu}.axmodel"
    # 量化精度摘要：precision_analysis 表最后几行（模型输出层的余弦相似度）
    grep -E "^│" "$WORK/${n}_${npu}.log" | tail -3 || true
  done
done
ls -la "$OUT"
