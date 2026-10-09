#!/usr/bin/env bash
# AX630C 板端交叉编译 (aarch64-none-linux-gnu gcc 9.2, glibc 2.30 sysroot; AX620E SDK V2.0 arm64_glibc)
# 需要:
#   TOOLCHAIN_DIR : gcc-arm-9.2-2019.12-x86_64-aarch64-none-linux-gnu 解压目录
#   BSP_MSP_DIR   : ax620e_bsp_sdk/msp/out/arm64_glibc (https://github.com/AXERA-TECH/ax620e_bsp_sdk)
#   OPENCV_DIR    : aarch64 OpenCV 的 lib/cmake/opencv4（默认用 scripts/build_3rdparty_aarch64.sh 编出来的，带 FFmpeg）
set -euo pipefail
cd "$(dirname "$0")"
: "${TOOLCHAIN_DIR:?set TOOLCHAIN_DIR}"
: "${BSP_MSP_DIR:?set BSP_MSP_DIR}"
OPENCV_DIR=${OPENCV_DIR:-$PWD/3rdparty/aarch64/lib/cmake/opencv4}
cmake -S . -B build/aarch64 -DBACKEND=AX -DBSP_MSP_DIR="$BSP_MSP_DIR" -DOpenCV_DIR="$OPENCV_DIR" \
  -DCMAKE_TOOLCHAIN_FILE="$PWD/cmake/aarch64-none-linux-gnu.toolchain.cmake" -DTOOLCHAIN_DIR="$TOOLCHAIN_DIR" \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build/aarch64 -j"$(nproc)"
"$TOOLCHAIN_DIR/bin/aarch64-none-linux-gnu-strip" -o build/aarch64/plate_face_demo.stripped build/aarch64/plate_face_demo
echo "OK: build/aarch64/plate_face_demo"
