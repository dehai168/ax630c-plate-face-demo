#!/usr/bin/env bash
# 打包: 板端运行包 + 源码包
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
OUTDIR=${OUTDIR:-$(dirname "$ROOT")}
NAME=ax630c_plate_face_demo
STAGE=$ROOT/build/stage/${NAME}_board
rm -rf "$STAGE"; mkdir -p "$STAGE/lib" "$STAGE/models" "$STAGE/lib_sdk_fallback"
cp "$ROOT/build/aarch64/plate_face_demo.stripped" "$STAGE/plate_face_demo"
cp "$ROOT/build/aarch64/video_io_selftest" "$STAGE/video_io_selftest"
"${TOOLCHAIN_DIR:?}/bin/aarch64-none-linux-gnu-strip" "$STAGE/video_io_selftest"
# FFmpeg 动态库（LGPL-2.1，动态链接；源码: https://ffmpeg.org/releases/ffmpeg-6.1.2.tar.xz，配置见 scripts/build_3rdparty_aarch64.sh）
for l in libavcodec.so.60 libavformat.so.60 libavutil.so.58 libswscale.so.7 libswresample.so.4; do
  cp -L "$ROOT/3rdparty/aarch64/lib/$l" "$STAGE/lib/"
done
# AX620E SDK V2.0.0_P7 的 AX 运行库，仅在板子系统里找不到 libax_engine.so 时备用（版本需与板子 BSP/驱动一致）
if [ -n "${BSP_MSP_DIR:-}" ]; then
  for l in libax_engine.so libax_interpreter.so libax_sys.so; do cp "$BSP_MSP_DIR/lib/$l" "$STAGE/lib_sdk_fallback/"; done
fi
cp "$ROOT"/models/axmodel/*.axmodel "$STAGE/models/"
cp "$ROOT/board/run.sh" "$ROOT/board/demo.cfg" "$STAGE/"
cp "$ROOT/test_data/test_video.mp4" "$STAGE/" 2>/dev/null || true
cp "$ROOT/board/README_board.md" "$STAGE/README.md"
cp -r "$ROOT/third_party_licenses" "$STAGE/"
tar -C "$(dirname "$STAGE")" -czf "$OUTDIR/${NAME}_board.tar.gz" "$(basename "$STAGE")"
echo "board package: $OUTDIR/${NAME}_board.tar.gz"
# 源码包（不含 build/ 3rdparty/ 大文件结果视频）
tar -C "$(dirname "$ROOT")" -czf "$OUTDIR/${NAME}_src.tar.gz" \
  --exclude="$NAME/build" --exclude="$NAME/3rdparty" --exclude="$NAME/results/*/*.mp4" --exclude="$NAME/results/*/snaps" \
  --exclude="__pycache__" "$NAME"
echo "source package: $OUTDIR/${NAME}_src.tar.gz"
