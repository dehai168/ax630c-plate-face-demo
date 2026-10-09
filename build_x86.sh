#!/usr/bin/env bash
# x86 (PC) 构建：ONNX Runtime 后端，用于在 PC 上验证前后处理/跟踪/叠加
# 依赖: OpenCV(带 FFmpeg, apt install libopencv-dev), onnxruntime-linux-x64 预编译包
set -euo pipefail
cd "$(dirname "$0")"
ORT_DIR=${ORT_DIR:-$PWD/3rdparty/onnxruntime-linux-x64-1.20.1}
if [ ! -d "$ORT_DIR" ]; then
  mkdir -p 3rdparty && curl -L https://github.com/microsoft/onnxruntime/releases/download/v1.20.1/onnxruntime-linux-x64-1.20.1.tgz | tar xz -C 3rdparty
fi
cmake -S . -B build/x86 -DBACKEND=ORT -DORT_DIR="$ORT_DIR" -DCMAKE_BUILD_TYPE=Release
cmake --build build/x86 -j"$(nproc)"
echo "OK: build/x86/plate_face_demo"
