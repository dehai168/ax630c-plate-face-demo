#!/bin/bash
# 编译 x86 调试工具（image_eval / dump_dets），需先完成 build/x86 的 cmake 构建
set -e
cd "$(dirname "$0")/.."
O=build/x86/CMakeFiles/plate_face_demo.dir/src
ORT=3rdparty/onnxruntime-linux-x64-1.20.1
for t in image_eval dump_dets test_rules; do
  c++ -O2 -std=gnu++14 -DPFD_BACKEND_ORT -Isrc -I$ORT/include -isystem /usr/include/opencv4 tools/$t.cpp \
    $O/detector.cpp.o $O/plate_rec.cpp.o $O/text_render.cpp.o $O/filters.cpp.o $O/infer/engine_ort.cpp.o \
    -o build/x86/$t -L$ORT/lib -Wl,-rpath,$(pwd)/$ORT/lib -lonnxruntime -lopencv_imgcodecs -lopencv_imgproc -lopencv_videoio -lopencv_core
done
echo OK
